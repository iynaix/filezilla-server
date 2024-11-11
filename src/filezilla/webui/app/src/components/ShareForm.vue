<template>
    <q-card>
        <q-card-section class="bg-primary text-white">
            <div v-if="!shareLink" class="text-h6">Create a public link</div>
            <div v-if="shareLink" class="text-h6">Public link created</div>

            <div class="text-subtitle2 ellipsis" style="max-width: 20em">
                for
                <q>{{
                    resourcePath.substring(resourcePath.lastIndexOf('/') + 1)
                }}</q>
            </div>
        </q-card-section>

        <q-separator />

        <q-card-section v-if="shareLink">
            <q-input
                type="url"
                v-model="shareLink"
                outlined
                hide-bottom-space
                standout
                readonly
                label="The link has been copied!"
                class="shadow-1"
                ref="shareLinkElement"
                style="min-width: 20em"
            >
            </q-input>
        </q-card-section>

        <q-card-section v-if="!shareLink">
            <q-form @submit="submit" greedy ref="form">
                <q-list>
                    <q-item tag="label" v-ripple>
                        <q-item-section>
                            <q-item-label>Expiration</q-item-label>
                            <q-item-label caption
                                >Disable this link on a specific
                                date.</q-item-label
                            >
                        </q-item-section>
                        <q-item-section avatar>
                            <q-toggle
                                checked-icon="check_circle"
                                unchecked-icon="cancel"
                                color="primary"
                                v-model="expires"
                            />
                        </q-item-section>
                    </q-item>
                    <q-item v-if="expires">
                        <q-item-section>
                            <q-input
                                outlined
                                label="Expiration date"
                                v-model="selectedDate"
                                :rules="[
                                    (val) =>
                                        (val && limitDate(val)) ||
                                        'Invalid date.',
                                ]"
                                input-class="cursor-pointer"
                                mask="####-##-##"
                            >
                                <q-popup-proxy ref="dateProxy">
                                    <q-date
                                        v-model="selectedDate"
                                        minimal
                                        @update:model-value="dateProxy?.hide()"
                                        no-unset
                                        mask="YYYY-MM-DD"
                                        :options="limitDate"
                                        :default-year-month="minYearMonth()"
                                    >
                                        <div
                                            class="row items-center justify-end"
                                        >
                                            <q-btn
                                                v-close-popup
                                                label="Close"
                                                color="primary"
                                                flat
                                            ></q-btn>
                                        </div>
                                    </q-date>
                                </q-popup-proxy>
                                <template v-slot:append>
                                    <q-icon
                                        name="event"
                                        class="cursor-pointer"
                                    ></q-icon>
                                </template>
                            </q-input>
                        </q-item-section>
                    </q-item>
                    <q-item tag="label" v-ripple>
                        <q-item-section>
                            <q-item-label>Require password</q-item-label>
                            <q-item-label caption
                                >Set a password to limit access to this resource
                                via link.</q-item-label
                            >
                        </q-item-section>
                        <q-item-section avatar>
                            <q-toggle
                                checked-icon="check_circle"
                                unchecked-icon="cancel"
                                color="primary"
                                v-model="passwordProtected"
                            />
                        </q-item-section>
                    </q-item>
                    <q-item v-if="passwordProtected">
                        <q-item-section>
                            <q-input
                                autocomplete="new-password"
                                v-model="password"
                                :type="showPassword ? 'text' : 'password'"
                                label="Password"
                                outlined
                                :rules="[
                                    (val) =>
                                        val.length > 0 ||
                                        'Password cannot be empty',
                                ]"
                                required
                                ><template v-slot:append>
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
                        </q-item-section>
                    </q-item>
                </q-list>
            </q-form>
        </q-card-section>

        <q-card-actions v-if="!shareLink" align="right">
            <q-btn
                flat
                label="Create the link"
                color="primary"
                type="submit"
                @click="submit"
            />
        </q-card-actions>
    </q-card>
</template>

<script setup lang="ts">
import { QPopupProxy } from 'quasar';
import { ref, watch, onMounted } from 'vue';
import auth from '../api/auth';
import { useQuasar, date } from 'quasar';

const props = defineProps({
    resourcePath: {
        type: String,
        required: true,
    },
});

const dateProxy = ref<QPopupProxy>();
const selectedDate = ref('');
const passwordProtected = ref(false);
const expires = ref(false);
const password = ref('');
const form = ref();
const shareLink = ref('');
const showPassword = ref(false);

const $q = useQuasar();
const shareLinkElement = ref<HTMLInputElement>();

function limitDate(date: string) {
    return Date.parse(date) - Date.now() >= 0;
}

function minYearMonth() {
    const d = new Date(Date.now());
    d.setDate(d.getDate() + 1);

    return date.formatDate(d, 'YYYY/MM');
}

function updateShareLinkElement() {
    if (shareLink.value && shareLinkElement.value) {
        shareLinkElement.value.focus();
        shareLinkElement.value.select();
        navigator.clipboard.writeText(shareLink.value);
        $q.notify({
            type: 'info',
            message: 'The link has been copied to the clipboard.',
        });
    }
}

watch(shareLink, () => {
    updateShareLinkElement();
});

watch(shareLinkElement, () => {
    updateShareLinkElement();
});

async function submit() {
    const success = await form.value?.validate(true);
    if (!success) {
        return;
    }

    if (!passwordProtected.value) {
        password.value = '';
    }

    if (!expires.value) {
        selectedDate.value = '';
    }

    if (props.resourcePath.startsWith('/home')) {
        let path = props.resourcePath.substring('/home'.length);
        if (path === '') {
            path = '/';
        }

        let expires_in = Math.floor(
            (Date.parse(selectedDate.value) - Date.now()) / 1000,
        );

        console.log('E: ', expires_in);

        const response = await auth.fetch('/api/v1/files/shares/', {
            method: 'POST',
            body: new URLSearchParams({
                path: decodeURIComponent(path),
                expires_in: expires_in ? expires_in.toString() : '',
                password: password.value,
            }),
        });

        if (response.ok) {
            const { share_token } = await response.json();
            if (share_token) {
                shareLink.value = `${window.location.origin}/shares/${share_token}`;
                return;
            }
        }

        $q.notify({
            type: 'negative',
            message: 'There was a problem getting the share link. Try again.',
        });

        return;
    }

    $q.notify({
        type: 'negative',
        message: 'Impossible to get the share link',
    });
}

onMounted(() => {
    if (props.resourcePath.startsWith('/shares/')) {
        // We already have the share link
        shareLink.value = `${window.location.origin}${props.resourcePath}`;
    }
});
</script>
