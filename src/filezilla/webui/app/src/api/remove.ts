import auth from './auth';

async function remove(path: string, recursive: boolean = true) {
    return await auth.fetch('/api/v1/files' + path, {
        method: 'DELETE',
        headers: recursive
            ? {
                  'X-FZ-Recursive': 'true',
              }
            : undefined,
    });
}

export default remove;
