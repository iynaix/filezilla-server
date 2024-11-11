import { LoginError } from './errors';

const login = async (username: string, password: string): Promise<void> => {
    const response: Response = await fetch('/api/v1/auth/token', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: new URLSearchParams({
            grant_type: 'password',
            username,
            password,
            cookie_path: '/api/v1/files',
        }),
    });

    if (!response.ok) {
        throw new LoginError();
    }

    localStorage.setItem('isLoggedIn', 'yes');
};

export default login;
