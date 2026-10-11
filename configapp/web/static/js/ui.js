import { api } from './api.js';
import { state } from './state.js';
import { generatePatch } from './patch-generator.js';
import { SchemaRenderer } from './schema-renderer.js';

export class UIController {
    constructor() {
        this.renderer = new SchemaRenderer('config-editor-container');
        this._bindEvents();
        this._startStatusPolling();
        
        state.subscribe(() => this._updateView());
    }

    _bindEvents() {
        document.getElementById('login-form').addEventListener('submit', async (e) => {
            e.preventDefault();
            await this._handleLogin();
        });

        document.getElementById('btn-logout').addEventListener('click', () => {
            api.clearTokens();
            location.reload();
        });

        document.getElementById('btn-cancel').addEventListener('click', () => {
            state.resetToInitial();
        });

        document.getElementById('btn-save').addEventListener('click', async () => {
            await this._handleSave();
        });

        document.getElementById('btn-restart-service').addEventListener('click', async () => {
            await this._handleRestart();
        });
    }

    async _handleLogin() {
        const username = document.getElementById('username').value;
        const password = document.getElementById('password').value;
        const errorEl = document.getElementById('auth-error');

        try {
            const res = await api.request('/auth/login', {
                method: 'POST',
                body: JSON.stringify({ username, password })
            });
            if (res.ok) {
                const data = await res.json();
                api.setTokens(data.access_token, data.refresh_token);
                document.getElementById('auth-screen').classList.add('hidden');
                document.getElementById('app-screen').classList.remove('hidden');
                await this._loadConfig();
            } else {
                errorEl.textContent = 'Invalid credentials';
            }
        } catch (err) {
            errorEl.textContent = 'Connection error';
        }
    }

    async _loadConfig() {
        try {
            const res = await api.request('/config');
            if (res.ok) {
                const config = await res.json();
                const etag = res.headers.get('ETag');
                state.setConfig(config, etag);
            }
        } catch (err) {
            console.error('Failed to load config', err);
        }
    }

    async _handleSave() {
        const patch = generatePatch(state.initialConfig, state.currentConfig);
        if (patch.length === 0) {
            alert('No changes to save.');
            return;
        }

        try {
            const res = await api.request('/config', {
                method: 'PATCH',
                headers: { 'If-Match': state.currentEtag },
                body: JSON.stringify(patch)
            });

            if (res.ok) {
                const newConfig = await res.json();
                const newEtag = res.headers.get('ETag');
                state.setConfig(newConfig, newEtag);
                alert('Configuration saved and service reloaded.');
            } else if (res.status === 409) {
                alert('Conflict: Configuration was modified by another user. Please reload.');
                await this._loadConfig();
            } else {
                const err = await res.json();
                alert(`Error: ${err.error}`);
            }
        } catch (err) {
            alert('Network error while saving.');
        }
    }

    async _handleRestart() {
        if (!confirm('Are you sure you want to restart the service?')) return;
        try {
            await api.request('/service/restart', { method: 'POST' });
            alert('Restart initiated.');
        } catch (err) {
            alert('Failed to restart service.');
        }
    }

    _startStatusPolling() {
        const poll = async () => {
            if (!api.accessToken) return;
            try {
                const res = await api.request('/service/status');
                if (res.ok) {
                    state.setServiceStatus(await res.json());
                }
            } catch (e) { /* ignore */ }
        };
        poll();
        setInterval(poll, 5000);
    }

    _updateView() {
        // Обновление редактора
        if (state.currentConfig) {
            this.renderer.render(state.currentConfig);
        }

        // Обновление статуса сервиса
        const statusEl = document.getElementById('service-status');
        const textEl = document.getElementById('status-text');
        const status = state.serviceStatus;
        
        statusEl.className = `status-indicator ${status.is_active ? 'active' : 'inactive'}`;
        textEl.textContent = `${status.status_text} (PID: ${status.pid || 'N/A'})`;
    }
}