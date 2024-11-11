function isLoggedIn(): boolean {
    return localStorage.getItem('isLoggedIn') === 'yes';
}

export default isLoggedIn;
