<script setup>
import { icons } from "../icons.js";

defineProps({
  name: { type: String, required: true },
  size: { type: [Number, String], default: 18 },
});
</script>

<template>
  <svg
    :width="size"
    :height="size"
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    stroke-width="2"
    stroke-linecap="round"
    stroke-linejoin="round"
    class="app-icon"
    aria-hidden="true"
    v-html="icons[name] || ''"
  ></svg>
</template>
