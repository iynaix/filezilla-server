import { ref, watchSyncEffect } from 'vue';

export const useCatchLie = () => {
    const error = ref();

    watchSyncEffect(() => {
        if (error.value === undefined) {
            return;
        } else if (error.value instanceof Error) {
            throw error.value;
        } else {
            throw new Error(String(error.value));
        }
    });

    return <T>(promise: Promise<T>) => {
        promise.catch((err) => {
            console.log('Caught error: ', err);
            error.value = err;
        });
    };
};
