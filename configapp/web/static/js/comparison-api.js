import { api } from './api.js';

class ComparisonApiClient {
    async getLists() {
        const res = await api.request('/comparison/lists');
        if (!res.ok) throw new Error('Failed to fetch lists');
        return res.json();
    }

    async getRecords(listId) {
        const res = await api.request(`/comparison/records/${listId}`);
        if (!res.ok) throw new Error('Failed to fetch records');
        return res.json();
    }

    async getVersion(listId) {
        const res = await api.request(`/comparison/version/${listId}`);
        if (!res.ok) throw new Error('Failed to fetch version');
        return res.json();
    }

    async applyBatch(listId, version, operations) {
        const res = await api.request('/comparison/batch', {
            method: 'POST',
            body: JSON.stringify({ list_id: listId, version, operations })
        });
        if (!res.ok) {
            const err = await res.json();
            throw new Error(err.error || 'Batch apply failed');
        }
        return res.json();
    }
}

export const comparisonApi = new ComparisonApiClient();