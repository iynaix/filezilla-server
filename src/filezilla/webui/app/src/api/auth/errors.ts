export class AuthError extends Error {
    constructor(message: string) {
        super(message);
        this.name = 'AuthError';
    }
}

export class TokenRefreshError extends AuthError {
    constructor() {
        super('Token refresh failed');
        this.name = 'TokenRefreshError';
    }
}

export class LoginError extends AuthError {
    constructor() {
        super('Login failed');
        this.name = 'LoginError';
    }
}

export class LogoutError extends AuthError {
    constructor() {
        super('Logout failed');
        this.name = 'LogoutError';
    }
}

export class BasicError extends AuthError {
    constructor() {
        super('Requires Basic Authentication');
        this.name = 'BasicError';
    }
}

export default {
    Auth: AuthError,
    TokenRefresh: TokenRefreshError,
    Login: LoginError,
    Logout: LogoutError,
    Basic: BasicError,
};
