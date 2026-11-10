<script setup>
import { useAuthStore } from './stores/auth'
const auth = useAuthStore()
</script>

<template>
  <router-view />
</template>
