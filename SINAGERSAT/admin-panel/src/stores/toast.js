import { defineStore } from 'pinia'
import { ref } from 'vue'

let nextId = 0

export const useToastStore = defineStore('toast', () => {
  const toasts = ref([])

  function add(type, message) {
    const id = ++nextId
    toasts.value.push({ id, type, message })
    setTimeout(() => remove(id), 3500)
  }

  function remove(id) {
    toasts.value = toasts.value.filter(t => t.id !== id)
  }

  function success(msg) { add('success', msg) }
  function error(msg) { add('error', msg) }
  function info(msg) { add('info', msg) }

  return { toasts, add, remove, success, error, info }
})
