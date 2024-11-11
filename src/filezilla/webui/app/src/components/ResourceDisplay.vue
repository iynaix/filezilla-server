<template>
    <div class="row">
        <div class="col" style="flex: 1">
            <DirectoryDisplay
                v-if="currentDirectoryPath"
                :style="{
                    height: componentHeight,
                    'max-width': 'auto',
                }"
                :root="props.root"
                :path="currentDirectoryPath"
                @selected="onEntrySelected"
                @download-requested="onEntryDownloadRequested"
            ></DirectoryDisplay>
        </div>

        <q-separator vertical spaced="md" v-show="currentFilePath" />

        <div
            class="col-5 bg-white"
            :class="panelMustBeFullScreen() ? 'fit fullscreen' : void 0"
            v-show="currentFilePath"
        >
            <q-toolbar class="q-gutter-x-sm">
                <q-btn
                    dense
                    icon="download"
                    label="Download"
                    @click="downloadFile"
                ></q-btn>

                <q-btn dense icon="share" label="Share">
                    <q-popup-proxy>
                        <ShareForm :resource-path="currentFilePath"></ShareForm>
                    </q-popup-proxy>
                </q-btn>

                <q-space></q-space>

                <q-btn
                    v-if="panelCanExitFullScreen()"
                    flat
                    dense
                    :icon="panelFullScreen ? 'fullscreen_exit' : 'fullscreen'"
                    @click="panelFullScreen = !panelFullScreen"
                ></q-btn>
                <q-btn
                    v-if="panelCanClose()"
                    flat
                    dense
                    icon="close"
                    @click="closePanel"
                ></q-btn>
            </q-toolbar>
            <div class="q-mt-md">
                <ImageDisplay
                    v-if="currentFileType === FileType.Image"
                    :root="props.root"
                    :path="currentFilePath"
                />
                <UnknownDisplay
                    v-else
                    :root="props.root"
                    :path="currentFilePath"
                />
            </div>
        </div>
    </div>
    <q-resize-observer @resize="onResize" />
    <a :href="downloadLink" ref="downloadHelper" style="visibility: hidden"></a>
</template>

<script lang="ts" setup>
import DirectoryDisplay from './DirectoryDisplay.vue';
import ImageDisplay from './ImageDisplay.vue';
import UnknownDisplay from './UnknownDisplay.vue';
import ShareForm from './ShareForm.vue';

import type { Entry } from './DirectoryDisplay.vue';
export type { Entry };

import { onMounted, watch, ref } from 'vue';
import { useRoute, useRouter } from 'vue-router';

import auth from '../api/auth';
import { useCatchLie } from '../composables/catchlie';
import { nextTick } from 'process';
import { useQuasar } from 'quasar';

const props = defineProps({
    root: {
        type: String,
        required: true,
    },
});

enum FileType {
    Image = 'image',
    Unknown = 'unknown',
}

const panelFullScreen = ref(false);
const currentFileType = ref(FileType.Unknown);
const currentFilePath = ref('');
const currentDirectoryPath = ref('');
const currentFileName = ref('');
const currentDirectoryName = ref('');
const downloadLink = ref('');
const downloadHelper = ref<HTMLElement>();

const identifyResource = async (path: string) => {
    console.log('Identifying resource: ', props.root + path);

    const response = await auth.fetch(props.root + path, {
        method: 'HEAD',
        headers: {
            Accept: 'application/ndjson; q=1, */*; q=0.5',
        },
    });

    let newFileType = FileType.Unknown;
    let newFilePath = '';
    let newDirectoryPath = '';

    if (response.ok) {
        let contentType = response.headers.get('Content-Type');
        if (contentType) {
            contentType = contentType.split(';')[0].trim();

            if (contentType === 'application/ndjson') {
                newDirectoryPath = path;
            } else {
                newDirectoryPath = path.substring(0, path.lastIndexOf('/'));
                if (newDirectoryPath === '/shares') {
                    newDirectoryPath = '';
                }

                newFilePath = path;

                if (contentType.startsWith('image/')) {
                    newFileType = FileType.Image;
                }
            }
        }
    }

    currentFileType.value = newFileType;
    currentFilePath.value = newFilePath;
    currentDirectoryPath.value = newDirectoryPath;

    currentFileName.value = decodeURIComponent(
        newFilePath.substring(newFilePath.lastIndexOf('/') + 1),
    );
    currentDirectoryName.value = decodeURIComponent(
        newDirectoryPath.substring(newDirectoryPath.lastIndexOf('/') + 1),
    );
};

const catchLie = useCatchLie();

const route = useRoute();
const router = useRouter();
const $q = useQuasar();

watch(route, (newPath) => {
    catchLie(identifyResource(newPath.path));
});

onMounted(() => {
    catchLie(identifyResource(route.path));
});

const componentHeight = ref('');
function onResize(size: { height: number; width: number }) {
    componentHeight.value = `${size.height - 120}px`;
}

function closePanel() {
    currentFilePath.value = '';
    panelFullScreen.value = false;
    router.push(currentDirectoryPath.value);
}

function panelMustBeFullScreen() {
    return (
        panelFullScreen.value || $q.screen.lt.md || !currentDirectoryPath.value
    );
}

function panelCanClose() {
    return !!currentDirectoryName.value;
}

function panelCanExitFullScreen() {
    return !!currentDirectoryName.value && !$q.screen.lt.md;
}

async function asyncDownloadFile(path: string) {
    let downloadPath;
    let password;

    if (path.startsWith('/home')) {
        path = path.substring('/home'.length);

        let tokenGenPath = props.root + '/shares/';

        const byteArray = new Uint8Array(32);
        password = crypto.getRandomValues(byteArray).toString();
        const response = await auth.fetch(tokenGenPath, {
            method: 'POST',
            body: new URLSearchParams({
                path: decodeURIComponent(path),
                expires_in: '30',
                password: password,
            }),
        });

        if (response.ok) {
            const { share_token } = await response.json();
            if (share_token) {
                downloadPath = `/shares/${share_token}`;
            }
        }
    } else {
        downloadPath = path;
    }

    if (downloadPath) {
        downloadLink.value = `${props.root}${downloadPath}?download`;
        if (password) {
            downloadLink.value +=
                '&authorization=' + encodeURIComponent(btoa(password));
        }
        console.log('Download path: ', downloadLink.value);
        nextTick(() => {
            downloadHelper.value?.click();
        });
    }
}

function downloadFile() {
    if (currentFilePath.value == '') {
        console.error('currentFilePath is empty');
        return;
    }

    catchLie(asyncDownloadFile(currentFilePath.value));
}

function onEntrySelected(e: Entry) {
    router.push(currentDirectoryPath.value + '/' + encodeURIComponent(e.name));
}

function onEntryDownloadRequested(e: Entry) {
    catchLie(asyncDownloadFile(currentDirectoryPath.value + '/' + e.name));
}
</script>
