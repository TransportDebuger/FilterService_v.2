import pika
from pathlib import Path
from domain.ports.transport_gateway import TransportGateway
from domain.entities.excluded_file import ExcludedFile
from domain.entities.export_profile import ExportProfile

class RabbitMQTransportGateway(TransportGateway):
    """Публикация файлов в очередь RabbitMQ (AMQP)."""
    
    def send(self, file: ExcludedFile, profile: ExportProfile) -> bool:
        broker_url = profile.transport_config.get('broker_url')
        queue_name = profile.transport_config.get('queue_name')
        if not broker_url or not queue_name:
            return False
        
        try:
            with open(file.file_path, 'rb') as f:
                file_content = f.read()
            
            params = pika.URLParameters(broker_url)
            connection = pika.BlockingConnection(params)
            channel = connection.channel()
            channel.queue_declare(queue=queue_name, durable=True)
            
            properties = pika.BasicProperties(
                headers={
                    'source_name': file.source_name, 
                    'file_hash': file.file_hash, 
                    'file_name': Path(file.file_path).name
                },
                delivery_mode=2, # Персистентность сообщений
                content_type='application/xml'
            )
            
            channel.basic_publish(
                exchange='',
                routing_key=queue_name,
                body=file_content,
                properties=properties
            )
            connection.close()
            return True
        except pika.exceptions.AMQPError:
            return False