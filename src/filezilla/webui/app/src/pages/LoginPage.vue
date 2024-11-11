<template>
    <q-page class="login-page">
        <q-card class="login-card">
            <q-form :on-submit="handleLogin">
                <q-card-section class="bg-primary text-white">
                    <div class="text-h6">Welcome</div>
                </q-card-section>
                <q-separator />
                <q-card-section>
                    <q-input
                        outlined
                        v-model="username"
                        label="Username"
                        type="text"
                        autocomplete="username"
                        :rules="[(val) => !!val || 'Username is required']"
                    />
                    <q-input
                        outlined
                        v-model="password"
                        label="Password"
                        :type="showPassword ? 'text' : 'password'"
                        autocomplete="new-password"
                    >
                        <template v-slot:append>
                            <q-icon
                                :name="
                                    showPassword
                                        ? 'visibility_off'
                                        : 'visibility'
                                "
                                class="cursor-pointer"
                                @click="showPassword = !showPassword"
                            />
                        </template>
                    </q-input>
                </q-card-section>
                <q-card-actions align="right">
                    <q-btn
                        flat
                        type="submit"
                        label="Login"
                        color="primary"
                        @click="handleLogin"
                    />
                </q-card-actions>
            </q-form>
        </q-card>
    </q-page>
</template>

<script lang="ts" setup>
import { ref } from 'vue';
import { useRouter } from 'vue-router';
import { useQuasar } from 'quasar';
import auth from 'src/api/auth';

const username = ref('');
const password = ref('');
const router = useRouter();
const $q = useQuasar();
const showPassword = ref(false);

const handleLogin = async () => {
    try {
        await auth.login(username.value, password.value);
        const redirect = sessionStorage.getItem('redirectAfterLogin') || '/';
        sessionStorage.removeItem('redirectAfterLogin');
        router.replace(redirect);

        $q.notify({
            type: 'positive',
            message: 'Login successful',
        });
    } catch (error) {
        if (error instanceof auth.errors.Login) {
            $q.notify({
                type: 'negative',
                message:
                    'Login failed. Please check your credentials and try again.',
            });
        } else {
            // Rethrow the error to be caught by the ErrorBoundary
            throw error;
        }
    }
};
</script>

<style scoped>
.login-page {
    display: flex;
    justify-content: center;
    align-items: center;
    height: 100vh;
}

.login-card {
    max-width: 10cm;
    width: 80%;
}
</style>
