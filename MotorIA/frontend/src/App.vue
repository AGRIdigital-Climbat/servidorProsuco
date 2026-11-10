<script setup>
import Sidebar from "./components/Sidebar.vue";
import DocDetailModal from "./components/DocDetailModal.vue";
import DocEditModal from "./components/DocEditModal.vue";
import TipoModal from "./components/TipoModal.vue";
</script>

<template>
  <div class="app">
    <Sidebar />

    <main class="main">
      <KeepAlive>
        <RouterView />
      </KeepAlive>
    </main>
  </div>

  <DocDetailModal />
  <DocEditModal />
  <TipoModal />
</template>
