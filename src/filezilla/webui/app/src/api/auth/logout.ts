import { LogoutError } from './errors';

const logout = async (): Promise<void> => {
    const response: Response = await fetch('/api/v1/auth/revoke', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: new URLSearchParams({
            token: 'cookie:refresh_token',
            hint: 'access_token',
        }),
    });

    if (!response.ok) {
        throw new LogoutError();
    }

    localStorage.removeItem('isLoggedIn');
};

export default logout;
