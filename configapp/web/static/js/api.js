class ApiClient {
    constructor() {
        this.baseUrl = '/api/v1';
        this.accessToken = localStorage.getItem('access_token');
    }

    setTokens(access, refresh) {
        this.accessToken = access;
        localStorage.setItem('access_token', access);
        localStorage.setItem('refresh_token', refresh);
    }

    clearTokens() {
        this.accessToken = null;
        localStorage.removeItem('access_token');
        localStorage.removeItem('refresh_token');
    }

    async request(endpoint, options = {}) {
        const url = `${this.baseUrl}${endpoint}`;
        const headers = { 'Content-Type': 'application/json', ...(options.headers || {}) };
        
        if (this.accessToken) {
            headers['Authorization'] = `Bearer ${this.accessToken}`;
        }

        try {
            const response = await fetch(url, { ...options, headers });
            
            if (response.status === 401) {
                // Попытка обновить токен
                const refreshed = await this._tryRefresh();
                if (refreshed) {
                    headers['Authorization'] = `Bearer ${this.accessToken}`;
                    return fetch(url, { ...options, headers });
                }
                throw new Error('Unauthorized');
            }
            return response;
        } catch (error) {
            console.error('API Error:', error);
            throw error;
        }
    }

    async _tryRefresh() {
        const refreshToken = localStorage.getItem('refresh_token');
        if (!refreshToken) return false;

        try {
            const res = await fetch(`${this.baseUrl}/auth/refresh`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ refresh_token: refreshToken })
            });
            if (res.ok) {
                const data = await res.json();
                this.setTokens(data.access_token, data.refresh_token);
                return true;
            }
        } catch (e) { /* ignore */ }
        return false;
    }
}

export const api = new ApiClient();