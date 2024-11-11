<template>
    <q-card>
        <q-form @submit="createFolder" greedy ref="form">
            <q-card-section>
                <q-input
                    outlined
                    type="text"
                    label="New folder's name"
                    v-model="newFolder"
                    :error="hasError"
                    :error-message="errorMessage"
                    lazy-rule
                    :rules="[validateFolder]"
                ></q-input>
            </q-card-section>
            <q-card-actions align="right">
                <q-btn
                    dense
                    flat
                    type="submit"
                    color="primary"
                    label="Create"
                    @click="createFolder"
                />
            </q-card-actions>
        </q-form>
    </q-card>
</template>

<script lang="ts" setup>
import { ref } from 'vue';
import { useCatchLie } from '../composables/catchlie';
import api from '../api';
import { QForm } from 'quasar';

const form = ref<QForm>();

const catchLie = useCatchLie();

const props = defineProps<{
    path: string;
}>();

const emit = defineEmits<{
    (event: 'created'): void;
}>();

const newFolder = ref('');
const hasError = ref(false);
const errorMessage = ref('');

function validateFolder(name: string) {
    if (!name) {
        return 'Name cannot be empty';
    }

    return true;
}

function createFolder() {
    const create = async () => {
        const response = await api.mkdir(
            `${props.path}/${encodeURIComponent(newFolder.value)}`,
        );
        if (response.ok) {
            hasError.value = false;
            emit('created');
        } else {
            errorMessage.value = response.statusText;
            hasError.value = true;
        }
    };

    form.value?.validate(true).then((success) => {
        if (success) {
            catchLie(create());
        }
    });
}
</script>
