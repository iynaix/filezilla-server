<template>
    <q-uploader
        ref="uploaderComponent"
        :with-credentials="withCredentials"
        :headers="getUploadHeaders"
        :label="`Upload a file to ${props.path.substring(props.path.lastIndexOf('/') + 1)}`"
        method="PUT"
        :url="getUploadUrl"
        send-raw
        @failed="handleFailedUpload"
        @uploaded="$emit('uploaded')"
        auto-upload
        hide-upload-btn
        no-thumbnails
    ></q-uploader>
</template>

<script lang="ts" setup>
import { ref } from 'vue';
import { QUploader } from 'quasar';
import { useCatchLie } from '../composables/catchlie';
import auth from '../api/auth';

const catchLie = useCatchLie();
const props = defineProps<{
    path: string;
}>();

defineEmits<{
    (event: 'uploaded'): void;
}>();

const uploaderComponent = ref<QUploader>();

function getUploadUrl(files: File[]) {
    return `/api/v1/files${props.path}/${files[0].name}`;
}

interface FailedUploadInfo {
    files: readonly File[];
    xhr: XMLHttpRequest;
}

function handleFailedUpload(info: FailedUploadInfo) {
    const refresh = async () => {
        await auth.refresh();
        uploaderComponent.value?.upload();
    };

    if (info.xhr.status == 401 && auth.isNeeded(`/api/v1/files${props.path}`)) {
        catchLie(refresh());
    }
}

function withCredentials() {
    return auth.isNeeded(`/api/v1/files${props.path}`);
}

function getUploadHeaders() {
    if (auth.isNeeded(`/api/v1/files${props.path}`)) {
        return [
            {
                name: 'Authorization',
                value: 'Bearer cookie:access_token',
            },
        ];
    }
}
</script>
