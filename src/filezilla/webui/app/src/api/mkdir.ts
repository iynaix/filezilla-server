import auth from './auth';

async function mkdir(path: string) {
    return auth.fetch('/api/v1/files' + path, {
        method: 'PUT',
        headers: {
            'X-FZ-Action': 'mkdir',
        },
    });
}

export default mkdir;
