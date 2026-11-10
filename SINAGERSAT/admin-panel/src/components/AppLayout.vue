<script setup>
import NavBar from './NavBar.vue'
import Toast from './Toast.vue'
</script>

<template>
  <div class="min-h-screen bg-slate-50">
    <NavBar />
    <main class="p-6 max-w-7xl mx-auto">
      <router-view />
    </main>
    <Toast />
  </div>
</template>
