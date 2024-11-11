<template>
    <div style="height: 100%">
        <q-img
            load
            fit="scale-down"
            :src="imageURL"
            alt="File Image"
            style="height: 100%"
        />
        <q-inner-loading :showing="loading">
            <q-spinner-gears size="50px" color="primary" />
        </q-inner-loading>
    </div>
</template>

<script lang="ts" setup>
import { ref, onMounted, watch, nextTick } from 'vue';
import { useCatchLie } from '../composables/catchlie';
import auth from '../api/auth';

const props = defineProps({
    root: {
        type: String,
        required: true,
    },
    path: {
        type: String,
        required: true,
    },
});

const imageURL = ref('');
const loading = ref(false);

const fetchImage = async (path: string) => {
    loading.value = true;
    nextTick(async () => {
        const response = await auth.fetch(props.root + path);
        const blob = await response.blob();
        imageURL.value = URL.createObjectURL(blob);
        loading.value = false;
    });
};

const catchLie = useCatchLie();

watch(
    () => props.path,
    (newPath) => {
        catchLie(fetchImage(newPath));
    },
);

onMounted(() => {
    catchLie(fetchImage(props.path));
});
</script>
