<script setup>
import { useToastStore } from '../stores/toast'

const toast = useToastStore()

const styles = {
  success: 'bg-emerald-600 text-white shadow-lg shadow-emerald-600/20',
  error: 'bg-rose-600 text-white shadow-lg shadow-rose-600/20',
  info: 'bg-indigo-600 text-white shadow-lg shadow-indigo-600/20',
}
</script>

<template>
  <div class="fixed top-4 right-4 z-50 flex flex-col gap-2 pointer-events-none">
    <TransitionGroup name="slide">
      <div v-for="t in toast.toasts" :key="t.id"
        :class="['flex items-center gap-3 px-4 py-3 rounded-xl text-sm font-medium pointer-events-auto transition-all duration-300', styles[t.type]]">
        <span>{{ t.type === 'success' ? '✓' : t.type === 'error' ? '✕' : 'ℹ' }}</span>
        <span>{{ t.message }}</span>
        <button @click="toast.remove(t.id)" class="ml-2 opacity-60 hover:opacity-100">✕</button>
      </div>
    </TransitionGroup>
  </div>
</template>
