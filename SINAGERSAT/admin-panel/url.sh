#!/bin/bash
# Muestra la URL/IP del admin-panel (Vite dev server)
# También muestra si el servidor está corriendo y cómo iniciarlo.

PORT=5173

# Detectar IP local principal
IP=$(ip -4 addr show | grep -oP '(?<=inet\s)\d+(\.\d+){3}' | grep -v '^127\.' | head -1)

if [ -z "$IP" ]; then
  IP="localhost"
fi

echo "======================================="
echo "  SINAGERSAT Admin Panel"
echo "======================================="
echo ""
echo "  Local:   http://localhost:$PORT"
echo "  Red:     http://$IP:$PORT"
echo ""

# Verificar si el servidor ya está corriendo
if curl -s -o /dev/null -w "" http://localhost:$PORT/ 2>/dev/null; then
  echo "  Estado:  ✅ CORRIENDO"
  echo ""
  echo "  Para abrir en el navegador:"
  echo "    sensible-browser http://$IP:$PORT"
  echo "    xdg-open http://localhost:$PORT"
else
  echo "  Estado:  ❌ DETENIDO"
  echo ""
echo "  Para iniciar (accesible desde la red):"
    echo "    cd /home/climbat/SINAGERSAT/admin-panel"
    echo "    npm run dev"
    echo ""
    echo "  O en segundo plano:"
    echo "    npm run dev &"
fi
echo "======================================="
