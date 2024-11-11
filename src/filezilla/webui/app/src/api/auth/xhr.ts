//import refreshToken from './refresh';
//import { BasicError } from './errors';
import { isAuthorizationNeeded } from './needed';

interface XhrOptions {
    method: 'GET' | 'POST' | 'PUT' | 'DELETE' | 'PATCH' | 'OPTIONS';
    headers?: { [key: string]: string };
    progressCallback?: (percentComplete: number) => void;
    responseType?: XMLHttpRequestResponseType;
    body?: Document | XMLHttpRequestBodyInit | null;
    onUnauthorized?: () => Promise<void>; // Callback to handle 401 responses and refresh credentials
}

function xhr(
    path: string,
    options: XhrOptions,
): Promise<string | ArrayBuffer | Blob | Document | object | null> {
    return new Promise((resolve, reject) => {
        if (!path.startsWith('/')) {
            throw Error('The path must be absolute');
        }

        const needsAuthorization = isAuthorizationNeeded(path);

        const request = new XMLHttpRequest();
        let hasRetried = false;

        const sendRequest = () => {
            request.open(options.method, path, true);

            // This would need to be set to false in case we wanted to deal with basic authentication ourselves.
            request.withCredentials = true;

            // Set responseType if provided
            if (options.responseType) {
                request.responseType = options.responseType;
            }

            // Set custom headers
            if (options.headers) {
                for (const [header, value] of Object.entries(options.headers)) {
                    request.setRequestHeader(header, value);
                }
            }

            if (needsAuthorization) {
                request.setRequestHeader(
                    'Authorization',
                    'Bearer cookie:access_token',
                );

                request.setRequestHeader('Cache-Control', 'no-store');
            }

            // Capture the progressCallback if provided
            const progressCallback = options.progressCallback;

            // Monitor progress
            if (progressCallback) {
                request.upload.onprogress = function (event: ProgressEvent) {
                    if (event.lengthComputable) {
                        const percentComplete =
                            (event.loaded / event.total) * 100;
                        progressCallback(percentComplete);
                    }
                };
            }

            // Handle successful request
            request.onload = function () {
                if (request.status >= 200 && request.status < 300) {
                    resolve(request.response);
                } else if (
                    needsAuthorization &&
                    request.status === 401 &&
                    options.onUnauthorized &&
                    !hasRetried
                ) {
                    // Handle 401 Unauthorized
                    hasRetried = true;
                    options
                        .onUnauthorized()
                        .then(() => {
                            // Retry the request after obtaining new credentials
                            sendRequest();
                        })
                        .catch(reject);
                } else {
                    reject(
                        new Error(
                            `Request failed with status: ${request.status} ${request.statusText}`,
                        ),
                    );
                }
            };

            // Handle errors
            request.onerror = function () {
                reject(
                    new Error(
                        `Network error: ${request.status} ${request.statusText}`,
                    ),
                );
            };

            request.send(options.body);
        };

        sendRequest();
    });
}

export default xhr;
