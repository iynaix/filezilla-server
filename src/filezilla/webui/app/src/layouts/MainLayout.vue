<template>
    <q-layout view="hhr lpR lfr">
        <q-header flat class="text-black transparent">
            <q-toolbar>
                <!--q-btn
                    flat
                    dense
                    round
                    icon="menu"
                    aria-label="Menu"
                    @click="toggleLeftDrawer"
                /-->

                <q-toolbar-title>
                    <q-avatar>
                        <img src="/icons/favicon-128x128.png" />
                    </q-avatar>
                    <q-badge
                        align="bottom"
                        color="secondary"
                        size="xs"
                        class="text-black"
                        >v{{ productVersion }}</q-badge
                    >
                </q-toolbar-title>
                <q-btn flat icon="logout" @click="logout">Leave</q-btn>
            </q-toolbar>
        </q-header>

        <!--q-drawer
            show-if-above
            v-model="leftDrawerOpen"
            side="left"
            bordered
            class="bg-transparent text-black"
            elevated
        >
        </q-drawer-->

        <q-page-container>
            <ErrorBoundary>
                <router-view v-slot="{ Component }">
                    <transition>
                        <keep-alive>
                            <component
                                :is="Component"
                                style="display: flex; flex-direction: column"
                                class="q-px-md"
                            />
                        </keep-alive>
                    </transition>
                </router-view>
            </ErrorBoundary>
        </q-page-container>
    </q-layout>
</template>

<script setup lang="ts">
defineOptions({
    name: 'MainLayout',
});

import auth from '../api/auth';

import { ref, onMounted } from 'vue';
import ErrorBoundary from 'src/components/ErrorBoundary.vue';
import { useRouter } from 'vue-router';
import { useQuasar } from 'quasar';

const router = useRouter();
const $q = useQuasar();

// Define refs to hold the meta tag values
const productName = ref<string>('');
const productVersion = ref<string>('');

// Function to get meta tag content
const getMetaContent = (name: string): string => {
    const meta = document.querySelector(`meta[name="${name}"]`);
    return meta ? meta.getAttribute('content') || '' : '';
};

async function logout() {
    await auth.logout();
    $q.notify({
        type: 'positive',
        message: 'Logged out successfully!',
    });
    router.push('/login');
}

// Use onMounted to set the values when the component is mounted
onMounted(() => {
    productName.value = getMetaContent('fz:product_name');
    productVersion.value = getMetaContent('fz:product_version');
});

/*
const leftDrawerOpen = ref(false);

function toggleLeftDrawer() {
    leftDrawerOpen.value = !leftDrawerOpen.value;
}*/
</script>
