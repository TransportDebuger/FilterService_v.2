import { UIController } from './ui.js';
import { api } from './api.js';

document.addEventListener('DOMContentLoaded', () => {
    // Проверка наличия токена при загрузке
    if (api.accessToken) {
        document.getElementById('auth-screen').classList.add('hidden');
        document.getElementById('app-screen').classList.remove('hidden');
    }
    
    // Инициализация UI
    new UIController();
});