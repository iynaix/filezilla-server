const protectedPath = /^\/api\/v1\/files/;
const unprotectedPath = /^\/api\/v1\/files\/shares\/./;

export function isAuthorizationNeeded(path: string): boolean {
    return !unprotectedPath.test(path) && protectedPath.test(path);
}

export default isAuthorizationNeeded;
