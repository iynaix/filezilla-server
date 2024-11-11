<template>
    <q-page>
        <q-toolbar>
            <q-breadcrumbs>
                <q-breadcrumbs-el icon="share" to="/shares" exact>
                    <span>Shares</span>
                </q-breadcrumbs-el>
                <q-breadcrumbs-el
                    v-for="crumb in crumbs"
                    :key="crumb.to"
                    :to="crumb.to"
                    exact
                >
                    <span>{{ crumb.label }}</span>
                </q-breadcrumbs-el>
                >
            </q-breadcrumbs>
        </q-toolbar>
        <ResourceDisplay root="/api/v1/files"></ResourceDisplay>
    </q-page>
</template>

<style scoped>
.q-router-link--exact-active span {
    font-weight: bolder;
}

.q-breadcrumbs__el span {
    color: var(--q-primary) !important;
    overflow: hidden !important;
    white-space: nowrap !important;
    text-overflow: ellipsis !important;
    max-width: 15em !important;
}
</style>

<script lang="ts" setup>
import { computed } from 'vue';
import ResourceDisplay from 'src/components/ResourceDisplay.vue';

const props = defineProps({
    path: {
        type: String,
        required: true,
    },
});

const crumbs = computed(() =>
    props.path
        .split('/')
        .filter((v) => v !== '')
        .map(
            (function () {
                let to = '/shares';
                return (crumb) => {
                    crumb = decodeURIComponent(crumb);
                    to = to + '/' + crumb;
                    return {
                        label: crumb,
                        to: to,
                    };
                };
            })(),
        ),
);
</script>
