import { TokenRefreshError } from './errors';

const refreshToken = async (): Promise<void> => {
    const response = await fetch('/api/v1/auth/token', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: new URLSearchParams({
            grant_type: 'refresh_token',
            refresh_token: 'cookie:refresh_token',
            cookie_path: '/api/v1/files',
        }),
    });

    if (!response.ok) {
        localStorage.removeItem('isLoggedIn');
        throw new TokenRefreshError();
    }
};

export default refreshToken;
