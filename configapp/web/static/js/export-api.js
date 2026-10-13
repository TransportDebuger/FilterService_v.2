import { api } from './api.js';

class ExportApiClient {
    // Profiles CRUD
    async getProfiles() {
        const res = await api.request('/export/profiles');
        if (!res.ok) throw new Error('Failed to fetch profiles');
        return res.json();
    }

    async saveProfile(profileData) {
        const res = await api.request('/export/profiles', {
            method: 'POST',
            body: JSON.stringify(profileData)
        });
        if (!res.ok) throw new Error('Failed to save profile');
        return res.json();
    }

    async deleteProfile(id) {
        const res = await api.request(`/export/profiles/${id}`, { method: 'DELETE' });
        if (!res.ok) throw new Error('Failed to delete profile');
    }

    // Manifest
    async getManifest() {
        const res = await api.request('/export/manifest');
        if (!res.ok) throw new Error('Failed to fetch manifest');
        return res.json();
    }

    async acknowledge(token) {
        const res = await api.request('/export/ack', {
            method: 'POST',
            body: JSON.stringify({ token })
        });
        if (!res.ok) throw new Error('Failed to send ACK');
    }
}

export const exportApi = new ExportApiClient();