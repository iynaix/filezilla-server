import auth from './auth';

async function rename(from: string, to: string) {
    const path = from.substring(0, from.lastIndexOf('/'));
    from = from.substring(path.length + 1);

    return await auth.fetch('/api/v1/files' + path, {
        method: 'POST',
        headers: {
            'X-FZ-Action': `move-from; path=${encodeURIComponent(from)}, move-to; path=${encodeURIComponent(to)}`,
        },
    });
}

export default rename;
