<script setup>
import AppIcon from "./AppIcon.vue";

defineProps({
  show: Boolean,
  doc: { type: Object, default: null },
  url: { type: String, default: "" },
});

const emit = defineEmits(["close"]);
</script>

<template>
  <Transition name="modal">
    <div v-if="show" class="modal-overlay" @click.self="emit('close')">
      <div class="modal pdf-modal">
        <div class="modal-header">
          <h3 class="pdf-title">{{ doc ? doc.titulo : "Vista previa del documento" }}</h3>
          <div class="pdf-head-actions">
            <a class="btn btn--ghost btn--sm" :href="url" download target="_blank" rel="noopener">
              <AppIcon name="download" :size="14" />
              Descargar
            </a>
            <button class="modal-close" @click="emit('close')">
              <AppIcon name="x" :size="18" />
            </button>
          </div>
        </div>
        <div class="modal-body pdf-body">
          <iframe
            v-if="url"
            :src="url"
            class="pdf-frame"
            title="Vista previa del documento"
          ></iframe>
          <p v-else class="empty-state">No se pudo cargar el documento.</p>
        </div>
      </div>
    </div>
  </Transition>
</template>

<style scoped>
.pdf-modal {
  width: min(980px, calc(100vw - 40px));
  max-height: 92vh;
}

.pdf-title {
  max-width: 520px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pdf-head-actions {
  display: flex;
  align-items: center;
  gap: 10px;
}

.pdf-body {
  padding: 0;
  height: calc(92vh - 66px);
}

.pdf-frame {
  width: 100%;
  height: 100%;
  border: 0;
  border-radius: 0 0 var(--radius-lg) var(--radius-lg);
  background: #fff;
}
</style>
