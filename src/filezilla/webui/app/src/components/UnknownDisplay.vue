<template class="q-pa-md">
    <q-card flat>
        <q-card-section>
            <div class="text-h6">Cannot display content</div>
        </q-card-section>
    </q-card>
</template>

<script lang="ts" setup>
defineProps({
    root: {
        type: String,
    },
    path: {
        type: String,
    },
});
</script>
