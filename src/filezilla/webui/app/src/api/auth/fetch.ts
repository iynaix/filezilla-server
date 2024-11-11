// src/auth/fetch.ts
import refreshToken from './refresh';
import { BasicError } from './errors';
import { isAuthorizationNeeded } from './needed';

const fetchWithAuth = async (
    path: string,
    options: RequestInit = {},
): Promise<Response> => {
    if (!path.startsWith('/')) {
        throw Error('The path must be absolute');
    }

    const needsAuthorization = isAuthorizationNeeded(path);

    if (needsAuthorization) {
        options.headers = new Headers(options.headers);
        options.headers.set('Authorization', 'Bearer cookie:access_token');
        options.cache = 'no-cache';
    } else {
        // Uncomment this if we want to deal with the basic authentication ourselves
        // options.credentials = 'omit';
    }

    let response = await fetch(path, options);

    if (response.status === 401) {
        if (needsAuthorization) {
            await refreshToken();
            response = await fetch(path, options);
        } else {
            const is_basic =
                response.headers.get('WWW-Authenticate')?.split(' ').at(0) ===
                'Basic';

            if (is_basic) {
                throw new BasicError();
            }
        }
    }

    return response;
};

export default fetchWithAuth;
