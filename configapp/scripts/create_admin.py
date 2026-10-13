import os
import bcrypt
from pathlib import Path
from dotenv import load_dotenv

from datasource.database import Database
from datasource.sqlite.sqlite_user_repository import SqliteUserRepository
from domain.entities.user import User

def create_admin_user():
    load_dotenv()
    db_path = Path(os.getenv('DATABASE_PATH', './data/app.db'))
    
    # Инициализация БД и репозитория
    db = Database(db_path)
    user_repo = SqliteUserRepository(db)

    username = "admin"
    password = "admin"

    password_bytes = password.encode('utf-8')
    salt = bcrypt.gensalt()
    hashed_password = bcrypt.hashpw(password_bytes, salt).decode('utf-8')

    user = User(username=username, password_hash=hashed_password)
    user_repo.save_user(user)
    
    print(f"Пользователь '{username}' успешно создан/обновлен в SQLite.")
    print(f"База данных: {db_path.resolve()}")

if __name__ == "__main__":
    create_admin_user()