<template>
    <div>
        <!-- Render the slot content if there's no error -->
        <slot v-if="!error"></slot>

        <!-- Show error message if an error is caught -->
        <div v-else>
            <q-page>
                <q-card>
                    <q-card-section>
                        <div class="text-h6">An error occurred</div>
                    </q-card-section>
                    <q-card-section>{{ error.message }}</q-card-section>
                    <q-card-actions align="right">
                        <q-btn
                            label="Go to Login"
                            color="primary"
                            @click="goToLogin"
                        />
                    </q-card-actions>
                </q-card>
            </q-page>
        </div>
    </div>
</template>

<script lang="ts" setup>
import { ref, onErrorCaptured } from 'vue';
import { useRouter } from 'vue-router';
import { useQuasar } from 'quasar';
import auth from 'src/api/auth';

const error = ref<Error | null>(null);
const router = useRouter();
const $q = useQuasar();

const goToLogin = () => {
    router.push('/login');
};

onErrorCaptured((err) => {
    if (err instanceof auth.errors.TokenRefresh) {
        $q.notify({
            type: 'negative',
            message: 'Session expired. Please log in again.',
        });
        router.push('/login');
    } else {
        error.value = err as Error;
    }
    return false;
});
</script>

<style scoped>
/* Add any necessary styles here */
</style>
