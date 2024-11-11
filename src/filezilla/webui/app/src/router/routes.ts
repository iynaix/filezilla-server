import { RouteRecordRaw } from 'vue-router';
import MainLayout from 'src/layouts/MainLayout.vue';
import LoginLayout from 'src/layouts/LoginLayout.vue';
import auth from 'src/api/auth';

const routes: RouteRecordRaw[] = [
    {
        path: '/',
        redirect: '/home',
    },
    {
        path: '/home/:path(.*)?',
        component: MainLayout,
        beforeEnter: (to, from, next) => {
            if (!auth.isLoggedIn()) {
                sessionStorage.setItem('redirectAfterLogin', to.fullPath);
                next({ name: 'Login' });
            } else {
                next();
            }
        },
        children: [
            {
                path: '',
                name: 'Home',
                component: () => import('pages/HomePage.vue'),
                props: true,
            },
        ],
    },
    {
        path: '/shares',
        redirect: '/',
    },
    {
        path: '/shares/:path(.+)',
        component: MainLayout,
        children: [
            {
                path: '',
                name: 'Shares',
                component: () => import('pages/SharePage.vue'),
                props: true,
            },
        ],
    },
    {
        path: '/login',
        component: LoginLayout,
        children: [
            {
                path: '',
                name: 'Login',
                component: () => import('pages/LoginPage.vue'),
            },
        ],
    },
    {
        path: '/:catchAll(.*)*',
        component: () => import('pages/ErrorNotFound.vue'),
    },
];

export default routes;
