#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore.h>
#include <random>
#include <chrono>
#include <atomic>

class JogoDasCadeiras {
public:
    JogoDasCadeiras(int n) 
        : num_cadeiras(n - 1), total_jogadores(n), jogo_em_andamento(true) {
        sem_init(&semaforo, 0, num_cadeiras);
    }

    ~JogoDasCadeiras() {
        sem_destroy(&semaforo);
    }

    void iniciar_rodada() {
        std::lock_guard<std::mutex> lock(mutex);
        num_cadeiras--; // Reduz o número de cadeiras a cada rodada
        sem_init(&semaforo, 0, num_cadeiras); // Ressincroniza o semáforo para o novo número de cadeiras
    }

    void parar_musica() {
        std::unique_lock<std::mutex> lock(mutex);
        std::cout << "A música parou! Tentem se sentar!\n";
        musica_parou = true;
        music_cv.notify_all();
    }

    void eliminar_jogador() {
        std::lock_guard<std::mutex> lock(mutex);
        total_jogadores--;
        if (total_jogadores == 1) {
            jogo_em_andamento = false;
            std::cout << "Temos um vencedor!\n";
        }
    }

    bool is_musica_parou() const { return musica_parou; }
    bool is_jogo_em_andamento() const { return jogo_em_andamento; }
    void reset_musica() { musica_parou = false; }

    sem_t semaforo;
    std::condition_variable music_cv;
    std::mutex mutex;
    
private:
    int num_cadeiras;
    int total_jogadores;
    bool musica_parou = false;
    bool jogo_em_andamento;
};

class Jogador {
public:
    Jogador(int id, JogoDasCadeiras &jogo) : id(id), jogo(jogo), eliminado(false) {}

    void tentar_ocupar_cadeira() {
        std::unique_lock<std::mutex> lock(jogo.mutex);
        jogo.music_cv.wait(lock, [&]() { return jogo.is_musica_parou(); });

        if (sem_trywait(&jogo.semaforo) == 0) {
            // Conseguiu sentar
            std::cout << "Jogador " << id << " conseguiu uma cadeira.\n";
        } else {
            // Foi eliminado
            eliminado = true;
            std::cout << "Jogador " << id << " foi eliminado!\n";
            jogo.eliminar_jogador();
        }
    }

    bool is_eliminado() const { return eliminado; }

private:
    int id;
    bool eliminado;
    JogoDasCadeiras &jogo;
};


class Coordenador {
public:
    Coordenador(JogoDasCadeiras &jogo) : jogo(jogo) {}

    void iniciar_jogo() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(1, 3); // Duração aleatória da música entre 1 e 3 segundos

        while (jogo.is_jogo_em_andamento()) {
            std::cout << "A música está tocando...\n";
            std::this_thread::sleep_for(std::chrono::seconds(dist(gen)));
            jogo.parar_musica();
            std::this_thread::sleep_for(std::chrono::seconds(1)); // Pausa para os jogadores tentarem se sentar

            jogo.iniciar_rodada();
            jogo.reset_musica();
        }
    }

private:
    JogoDasCadeiras &jogo;
};

 int main() {
    int num_jogadores = 5; // Número de jogadores (exemplo)
    JogoDasCadeiras jogo(num_jogadores);

    Coordenador coordenador(jogo);
    std::vector<Jogador> jogadores;
    for (int i = 0; i < num_jogadores; ++i) {
        jogadores.emplace_back(i + 1, jogo);
    }

    std::thread coord_thread(&Coordenador::iniciar_jogo, &coordenador);

    std::vector<std::thread> jogador_threads;
    for (auto &jogador : jogadores) {
        jogador_threads.emplace_back([&jogador]() {
            while (!jogador.is_eliminado()) {
                jogador.tentar_ocupar_cadeira();
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Espera para evitar loop muito rápido
            }
        });
    }

    coord_thread.join();
    for (auto &t : jogador_threads) {
        t.join();
    }

    return 0;
}
