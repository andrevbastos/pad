#!/bin/bash

JA_RESTAURADO=0
PAD_PID=""

restaurar_sistema() {
    if [ "$JA_RESTAURADO" -eq 1 ]; then
        return
    fi
    JA_RESTAURADO=1
    
    echo -e "\n==> Restaurando o sistema..."
    
    if [ -n "$PAD_PID" ]; then
        kill -9 $PAD_PID 2>/dev/null
    fi

    sudo systemctl unmask sleep.target suspend.target hibernate.target hybrid-sleep.target > /dev/null

    sudo rfkill unblock all
    sudo nmcli networking on
    
    if command -v cpupower &> /dev/null; then
        sudo cpupower frequency-set -g powersave > /dev/null
    fi
    
    if [ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
        echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo > /dev/null
    fi
    
    echo "==> Tudo voltou ao normal!"
}

trap 'echo -e "\n[!] Abortado pelo utilizador."; restaurar_sistema; exit 1' INT TERM
trap 'restaurar_sistema' EXIT

echo "==> Preparando ambiente quiescente (e bloqueando suspensão)..."

sudo systemctl mask sleep.target suspend.target hibernate.target hybrid-sleep.target > /dev/null

if command -v cpupower &> /dev/null; then
    sudo cpupower frequency-set -g performance > /dev/null
fi

if [ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
    echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo > /dev/null
fi

sudo nmcli networking off
sudo rfkill block all

for i in {1..5}
do
    echo "-> A aguardar 10 segundos para arrefecimento do CPU e libertação de cache..."
    sleep 10 &
    SLEEP_PID=$!
    wait $SLEEP_PID

    echo "-> A executar bateria PARALELA (Usando todos os núcleos)..."
    mkdir -p /home/andre/projects/pad/results/perf
    perf stat -e cache-misses,cache-references,L1-dcache-load-misses,LLC-load-misses,branches,branch-misses \
        -o /home/andre/projects/pad/results/perf/parallel.log \
        /home/andre/projects/pad/build/bin/pad bench paralelo &
    PAD_PID=$!
    wait $PAD_PID 

    echo "-> A aguardar 10 segundos para arrefecimento do CPU e libertação de cache..."
    sleep 10 &
    SLEEP_PID=$!
    wait $SLEEP_PID

    echo "-> A executar bateria SEQUENCIAL no núcleo 2 (Ambiente isolado)..."

    taskset -c 2 perf stat -e cache-misses,cache-references,L1-dcache-load-misses,LLC-load-misses,branches,branch-misses \
        -o /home/andre/projects/pad/results/perf/sequential.log \
        /home/andre/projects/pad/build/bin/pad bench sequencial &
    PAD_PID=$!
    wait $PAD_PID

    mkdir -p /home/andre/facul/programacao-de-alto-desempenho/bench/$i
    mv /home/andre/projects/pad/results/* /home/andre/facul/programacao-de-alto-desempenho/bench/$i
done

echo -e "\n==> Testes concluídos com sucesso!"