<script setup>
defineProps({
  show: Boolean,
  title: String,
  message: String,
  confirmText: { type: String, default: 'Confirmar' },
  cancelText: { type: String, default: 'Cancelar' },
  danger: Boolean,
})
const emit = defineEmits(['confirm', 'cancel'])
</script>

<template>
  <Teleport to="body">
    <Transition name="fade">
      <div v-if="show" class="fixed inset-0 z-50 flex items-center justify-center bg-black/40 backdrop-blur-sm">
        <div class="bg-white rounded-2xl shadow-2xl w-full max-w-sm mx-4 p-6 animate-[fadeIn_0.2s_ease-out]">
          <h3 class="text-lg font-bold text-slate-800 mb-2">{{ title }}</h3>
          <p class="text-sm text-slate-500 mb-6">{{ message }}</p>
          <div class="flex justify-end gap-3">
            <button @click="emit('cancel')"
              class="px-4 py-2 text-sm font-medium text-slate-600 hover:bg-slate-100 rounded-lg transition-all">{{ cancelText }}</button>
            <button @click="emit('confirm')"
              :class="['px-4 py-2 text-sm font-medium text-white rounded-lg transition-all shadow-sm', danger ? 'bg-rose-600 hover:bg-rose-700' : 'bg-indigo-600 hover:bg-indigo-700']">{{ confirmText }}</button>
          </div>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>
