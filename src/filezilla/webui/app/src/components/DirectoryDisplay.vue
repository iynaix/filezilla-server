<template>
    <div>
        <q-toolbar class="q-gutter-x-sm">
            <q-btn dense icon="upload" label="Upload">
                <q-popup-proxy>
                    <UploadForm
                        :path="path"
                        @uploaded="handleSuccessfulUpload"
                    />
                </q-popup-proxy>
            </q-btn>
            <q-btn dense icon="create_new_folder" label="New Folder">
                <q-popup-proxy
                    ><NewFolderForm
                        :path="path"
                        @created="handleFolderCreation"
                    ></NewFolderForm>
                </q-popup-proxy>
            </q-btn>
            <q-btn dense icon="share" label="Share">
                <q-popup-proxy>
                    <ShareForm :resource-path="path"></ShareForm>
                </q-popup-proxy>
            </q-btn>
        </q-toolbar>

        <q-table
            virtual-scroll
            flat
            :grid="isTableGrid"
            :dense="isTableDense"
            row-key="name"
            :rows="entries"
            :columns="columns"
            :visible-columns="visibleColumns"
            :pagination="pagination"
            :sort-method="sortRows"
            :loading="loading"
            binary-state-sort
            v-model:selected="selectedRows"
        >
            <template #body="tprops">
                <q-tr
                    :props="tprops"
                    @click="tprops.selected = !tprops.selected"
                    class="overlayed-row non-selectable"
                >
                    <!--q-td key="select">
                    <q-checkbox v-model="tprops.selected" color="primary" />
                </q-td-->
                    <q-td key="name" :props="tprops">
                        <span
                            class="cursor-pointer"
                            @click.stop="
                                !loading ? $emit('selected', tprops.row) : false
                            "
                        >
                            <q-icon
                                :name="
                                    tprops.row.type === 'd'
                                        ? 'folder'
                                        : 'insert_drive_file'
                                "
                                left
                                size="2em"
                                :color="
                                    tprops.row.type === 'd' ? 'primary' : void 0
                                "
                            />
                            {{ tprops.row.name }}</span
                        >
                        <q-popup-edit
                            v-if="renaming === tprops.row.name"
                            :ref="
                                (el: QPopupEdit) =>
                                    (renamePopup[tprops.rowIndex] = el)
                            "
                            v-model="tprops.row.name"
                            title="Edit the Name"
                            auto-save
                            v-slot="scope"
                        >
                            <q-input
                                v-model="scope.value"
                                dense
                                autofocus
                                counter
                                @keyup.enter="
                                    () => {
                                        api.rename(
                                            props.path + '/' + tprops.row.name,
                                            scope.value,
                                        ).then(() => {
                                            scope.set();
                                            fetchDirectory(props.path);
                                        });
                                    }
                                "
                            />
                        </q-popup-edit>
                    </q-td>
                    <q-td key="mtime" :props="tprops">
                        {{
                            date.formatDate(
                                tprops.row.mtime,
                                'YYYY-MM-DD HH:mm:ss',
                            )
                        }}
                    </q-td>
                    <q-td key="size" :props="tprops">
                        {{ formatSize(tprops.row.size) }}
                    </q-td>
                    <q-td key="actions" :props="tprops">
                        <q-btn dense @click.stop square flat icon="more_horiz">
                            <q-popup-proxy>
                                <q-list bordered class="bg-white">
                                    <q-item
                                        v-if="tprops.row.type !== 'd'"
                                        clickable
                                        v-ripple
                                        @click="
                                            $emit(
                                                'downloadRequested',
                                                tprops.row,
                                            )
                                        "
                                    >
                                        <q-item-section avatar>
                                            <q-icon name="download" />
                                        </q-item-section>

                                        <q-item-section
                                            >Download</q-item-section
                                        >
                                    </q-item>
                                    <q-item clickable v-ripple>
                                        <q-item-section avatar>
                                            <q-icon name="share" />
                                        </q-item-section>
                                        <q-item-section>Share</q-item-section>
                                        <q-popup-proxy>
                                            <ShareForm
                                                :resource-path="
                                                    props.path +
                                                    '/' +
                                                    tprops.row.name
                                                "
                                            ></ShareForm>
                                        </q-popup-proxy>
                                    </q-item>
                                    <q-item
                                        clickable
                                        v-ripple
                                        @click="deleteEntry(tprops.row)"
                                    >
                                        <q-item-section avatar>
                                            <q-icon name="delete" />
                                        </q-item-section>
                                        <q-item-section>Delete</q-item-section>
                                    </q-item>
                                    <q-item
                                        clickable
                                        v-ripple
                                        @click="
                                            renameEntry(
                                                tprops.rowIndex,
                                                tprops.row,
                                            )
                                        "
                                    >
                                        <q-item-section avatar>
                                            <q-icon name="text_fields" />
                                        </q-item-section>
                                        <q-item-section>Rename</q-item-section>
                                    </q-item>
                                </q-list>
                            </q-popup-proxy>
                        </q-btn>
                    </q-td>
                </q-tr>
            </template>
        </q-table>
    </div>
</template>

<script lang="ts" setup>
import { ref, onMounted, watch, computed, Ref } from 'vue';
import { QTableProps, date, useQuasar, QPopupProxy, QPopupEdit } from 'quasar';
import { useCatchLie } from '../composables/catchlie';
import ShareForm from './ShareForm.vue';
import UploadForm from './UploadForm.vue';
import NewFolderForm from './NewFolderForm.vue';

import auth from '../api/auth';
import api from '../api';
import { nextTick } from 'process';

export interface Entry {
    name: string;
    mtime: Date;
    type: string;
    size: number;
}

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

type Emits = {
    selected: [entry: Entry];
    downloadRequested: [entry: Entry];
    shareRequested: [entry: Entry];
};

defineEmits<Emits>();

const selectedRows = ref<Entry[]>();
const isTableGrid = ref<boolean>(false);
const isTableDense = ref<boolean>(false);
const entries = ref<Entry[]>([]);
const columns: QTableProps['columns'] = [
    {
        label: 'Name',
        field: 'name',
        name: 'name',
        align: 'left',
        sortable: true,
    },
    {
        label: 'Last modified',
        field: 'mtime',
        name: 'mtime',
        align: 'left',
        sortable: true,
        style: 'width: 50px',
        headerStyle: 'width: 50px',
    },
    {
        label: 'Size',
        field: 'size',
        name: 'size',
        align: 'left',
        sortable: true,
        style: 'width: 50px; overflow: hidden',
        headerStyle: 'width: 50px; overflow: hidden',
    },
    {
        label: '',
        field: '',
        name: 'actions',
        sortable: false,
        style: 'width: 50px; overflow: hidden',
        headerStyle: 'width: 50px; overflow: hidden',
    },
];

const $q = useQuasar();

const visibleColumns = computed(() => {
    let ret: string[] = ['name', 'actions'];

    if ($q.screen.gt.sm) {
        ret.push('size');
    }

    if ($q.screen.gt.xs) {
        ret.push('mtime');
    }

    return ret;
});

function formatSize(size: number): string {
    if (size < 0) {
        return '--';
    }

    return size.toString() + ' bytes';
}

const sortRows = (
    rows: readonly Entry[],
    sortBy: string,
    descending: boolean,
): readonly Entry[] => {
    const lt = descending ? 1 : -1;
    const gt = descending ? -1 : 1;

    if (sortBy == 'name') {
        return [...rows].sort((a: Entry, b: Entry): number => {
            if (a.type !== b.type) {
                return a.type < b.type ? lt : gt;
            }

            if (a.name !== b.name) {
                return a.name < b.name ? lt : gt;
            }
            return 0;
        });
    }

    if (sortBy == 'size') {
        return [...rows].sort((a: Entry, b: Entry): number => {
            if (a.size !== b.size) {
                return a.size < b.size ? lt : gt;
            }
            return 0;
        });
    }

    return [...rows].sort((a: Entry, b: Entry): number => {
        if (a.mtime !== b.mtime) {
            return a.mtime < b.mtime ? lt : gt;
        }

        return 0;
    });
};

const pagination = {
    sortBy: 'name',
    descending: false,
    page: 1,
    rowsPerPage: 25,
};

const loading = ref<boolean>(false);

const fetchDirectory = async (path: string) => {
    console.log('Fetching directory:', path);

    loading.value = true;

    if (path != '') {
        const response = await auth.fetch(props.root + path, {
            headers: {
                Accept: 'application/ndjson',
            },
        });

        if (!response.body) {
            throw new Error('Failed retrieving the directory content');
        }

        if (response.headers.get('Content-Type') !== 'application/ndjson') {
            throw new Error('Unrecognized Content-Type');
        }

        entries.value.length = 0;

        const reader = response.body.getReader();
        const decoder = new TextDecoder('utf-8');
        let runningText = '';

        while (true) {
            const { done, value } = await reader.read();
            if (done) {
                break;
            }

            const text = decoder.decode(value);
            const objects = text.split('\n');

            for (const obj of objects) {
                try {
                    runningText += obj;
                    const result = JSON.parse(runningText);
                    entries.value.push(result);
                    runningText = '';
                } catch (e) {
                    // Not a valid JSON object yet
                }
            }
        }
    } else {
        entries.value.length = 0;
    }

    loading.value = false;
};

const catchLie = useCatchLie();

watch(
    () => props.path,
    (newPath) => {
        catchLie(fetchDirectory(newPath));
    },
);

onMounted(() => {
    catchLie(fetchDirectory(props.path));
});

function handleSuccessfulUpload() {
    catchLie(fetchDirectory(props.path));
}

function handleFolderCreation() {
    catchLie(fetchDirectory(props.path));
}

function deleteEntry(e: Entry) {
    const type = e.type === 'd' ? 'folder' : 'file';

    $q.dialog({
        title: `Delete ${type}?`,
        message: `Are you sure you want to delete <strong>${e.name}</strong>?`,
        cancel: true,
        persistent: true,
        html: true,
    }).onOk(() => {
        api.remove(props.path + '/' + encodeURIComponent(e.name)).then(
            (response) => {
                if (response.ok) {
                    catchLie(fetchDirectory(props.path));
                } else {
                    $q.notify({
                        type: 'negative',
                        message: `Deleting the ${type} failed.`,
                    });
                }
            },
        );
    });
}

const renaming = ref('');
const renamePopup: Ref<QPopupEdit[]> = ref([]);

function renameEntry(i: number, e: Entry) {
    renaming.value = e.name;
    nextTick(() => {
        renamePopup.value[i].show();
    });
}
</script>
