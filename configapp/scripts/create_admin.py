import json
import os
import bcrypt
from pathlib import Path
from dotenv import load_dotenv

def create_admin_user():
    # Загрузка переменных окружения для определения пути к файлу пользователей
    load_dotenv()
    users_file_path = Path(os.getenv('USERS_FILE', 'users.json'))
    
    username = "admin"
    password = "admin"

    # Генерация хеша пароля
    password_bytes = password.encode('utf-8')
    salt = bcrypt.gensalt()
    hashed_password = bcrypt.hashpw(password_bytes, salt).decode('utf-8')

    # Чтение существующих данных (если файл уже существует)
    users_list = []
    if users_file_path.exists():
        try:
            with open(users_file_path, 'r', encoding='utf-8') as f:
                users_list = json.load(f)
            if not isinstance(users_list, list):
                users_list = []
        except (json.JSONDecodeError, IOError):
            users_list = []

    # Обновление или добавление пользователя
    user_exists = False
    for user in users_list:
        if user.get("username") == username:
            user["password_hash"] = hashed_password
            user_exists = True
            break
            
    if not user_exists:
        users_list.append({
            "username": username,
            "password_hash": hashed_password
        })

    # Атомарная запись в файл
    users_file_path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = users_file_path.with_suffix('.tmp')
    
    try:
        with open(temp_path, 'w', encoding='utf-8') as f:
            json.dump(users_list, f, ensure_ascii=False, indent=2)
        os.replace(temp_path, users_file_path)
        print(f"Пользователь '{username}' успешно создан/обновлен.")
        print(f"Файл сохранен: {users_file_path.resolve()}")
    except Exception as e:
        if temp_path.exists():
            os.unlink(temp_path)
        raise RuntimeError(f"Ошибка записи файла пользователей: {e}")

if __name__ == "__main__":
    create_admin_user()