<script setup>
import { ref, computed, watch } from 'vue'
import Modal from './Modal.vue'

const props = defineProps({
  columns: Array,
  data: Array,
  loading: Boolean,
  total: Number,
  searchable: Boolean,
  pageSize: { type: Number, default: 25 },
})

const emit = defineEmits(['edit', 'delete', 'create', 'search'])

const page = ref(1)
const perPage = ref(props.pageSize)
const search = ref('')
const showDeleteModal = ref(false)
const deleteTarget = ref(null)

watch(search, (v) => emit('search', v))

const totalPages = computed(() => Math.ceil((props.total || props.data?.length || 0) / perPage.value))

function confirmDelete(row) {
  deleteTarget.value = row
  showDeleteModal.value = true
}

function doDelete() {
  showDeleteModal.value = false
  if (deleteTarget.value) emit('delete', deleteTarget.value)
  deleteTarget.value = null
}
</script>

<template>
  <div>
    <!-- Search + actions bar -->
    <div v-if="searchable || $slots.actions" class="flex flex-wrap items-center gap-3 mb-4">
      <div v-if="searchable" class="relative flex-1 min-w-[200px] max-w-sm">
        <span class="absolute left-3 top-1/2 -translate-y-1/2 text-slate-400 text-sm">🔍</span>
        <input v-model="search" placeholder="Buscar..."
          class="w-full pl-9 pr-3 py-2 border border-slate-200 rounded-lg text-sm outline-none focus:ring-2 focus:ring-indigo-500 bg-white" />
      </div>
      <div class="flex-1"></div>
      <slot name="actions" />
    </div>

    <!-- Skeleton -->
    <div v-if="loading" class="bg-white border border-slate-200 rounded-xl overflow-hidden">
      <div class="space-y-0">
        <div v-for="i in 6" :key="i" class="flex gap-4 px-4" :class="i === 1 ? 'py-3 bg-slate-50 border-b border-slate-200' : 'py-2.5 border-b border-slate-100'">
          <div v-for="col in columns" :key="col.key" class="skeleton h-4" :style="{ width: col.key === 'id' || col.key?.includes('id_') ? '40px' : Math.random() > 0.5 ? '100px' : '80px' }"></div>
          <div class="skeleton h-4 w-16"></div>
        </div>
      </div>
    </div>

    <!-- Empty -->
    <div v-else-if="!data || data.length === 0" class="bg-white border border-slate-200 rounded-xl py-16 text-center">
      <div class="text-3xl text-slate-300 mb-3">∅</div>
      <p class="text-sm text-slate-400">Sin registros</p>
    </div>

    <!-- Table -->
    <div v-else class="bg-white border border-slate-200 rounded-xl overflow-hidden shadow-sm">
      <div class="overflow-x-auto">
        <table class="w-full text-sm">
          <thead>
            <tr class="bg-slate-50 border-b border-slate-200">
              <th v-for="col in columns" :key="col.key"
                class="text-left px-4 py-3 text-xs font-semibold text-slate-500 uppercase tracking-wider whitespace-nowrap">{{ col.label }}</th>
              <th v-if="$slots.actions" class="text-left px-4 py-3 text-xs font-semibold text-slate-500 uppercase tracking-wider whitespace-nowrap w-16"></th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(row, i) in data" :key="i"
              :class="['border-b border-slate-100 transition-all duration-150', i % 2 === 0 ? 'bg-white' : 'bg-slate-50/30', 'hover:bg-indigo-50/40']">
              <td v-for="col in columns" :key="col.key" class="px-4 py-2">
                <slot :name="'cell-' + col.key" :row="row" :value="row[col.key]">
                  <span :class="col.key === 'id' || col.key?.includes('id_') ? 'text-slate-400 font-mono text-xs' : 'text-slate-700'">{{ row[col.key] }}</span>
                </slot>
              </td>
              <td v-if="$slots.actions" class="px-4 py-2 text-right">
                <div class="relative inline-block">
                  <button @click="confirmDelete(row)" class="p-1.5 text-slate-400 hover:text-rose-600 hover:bg-rose-50 rounded-lg transition-all" title="Eliminar">🗑</button>
                  <slot name="actions" :row="row" />
                </div>
              </td>
            </tr>
          </tbody>
        </table>
      </div>

      <!-- Pagination -->
      <div class="flex items-center justify-between px-4 py-3 border-t border-slate-200 bg-slate-50/50">
        <div class="flex items-center gap-2 text-sm text-slate-500">
          <span>Mostrando</span>
          <select v-model.number="perPage" @change="page = 1"
            class="border border-slate-200 rounded-lg px-2 py-1 text-sm bg-white outline-none focus:ring-2 focus:ring-indigo-500">
            <option :value="10">10</option>
            <option :value="25">25</option>
            <option :value="50">50</option>
            <option :value="100">100</option>
          </select>
          <span>de {{ total || data.length }}</span>
        </div>
        <div class="flex items-center gap-1">
          <button @click="page = Math.max(1, page - 1)" :disabled="page <= 1"
            class="px-3 py-1.5 text-sm rounded-lg border border-slate-200 hover:bg-white transition-all disabled:opacity-30">←</button>
          <template v-for="p in totalPages" :key="p">
            <button v-if="p === 1 || p === totalPages || Math.abs(p - page) <= 1" @click="page = p"
              :class="['px-3 py-1.5 text-sm rounded-lg transition-all', p === page ? 'bg-indigo-600 text-white shadow-sm' : 'hover:bg-white border border-slate-200']">{{ p }}</button>
            <span v-else-if="p === page - 2 || p === page + 2" class="text-slate-300 px-1">...</span>
          </template>
          <button @click="page = Math.min(totalPages, page + 1)" :disabled="page >= totalPages"
            class="px-3 py-1.5 text-sm rounded-lg border border-slate-200 hover:bg-white transition-all disabled:opacity-30">→</button>
        </div>
      </div>
    </div>

    <!-- Total -->
    <div v-if="total !== undefined && !loading && data?.length" class="text-xs text-slate-400 mt-2">
      Total: {{ total }} registro(s)
    </div>

    <!-- Delete Modal -->
    <Modal :show="showDeleteModal" title="Confirmar eliminación" message="¿Estás seguro de eliminar este registro? Esta acción no se puede deshacer."
      confirmText="Eliminar" cancelText="Cancelar" :danger="true" @confirm="doDelete" @cancel="showDeleteModal = false" />
  </div>
</template>
