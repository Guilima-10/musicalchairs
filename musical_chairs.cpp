#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <algorithm>
#include <semaphore.h> // POSIX semáforo

using namespace std;

class JogoDasCadeiras {
public:
    int cadeiras;
    vector<string> players;
    vector<string> eliminados;
    sem_t semaphore;
    condition_variable music_cv;
    mutex mtx;
    bool musica_parou = false;
    bool primeira_rodada = true;  // Variável para rastrear se é a primeira rodada

    JogoDasCadeiras(int n) : cadeiras(n - 1) {
        for (int i = 1; i <= n; ++i) {
            players.push_back("P" + to_string(i));
        }
        sem_init(&semaphore, 0, cadeiras);
    }

    ~JogoDasCadeiras() {
        sem_destroy(&semaphore);
    }

    void iniciar_rodada() {
        unique_lock<mutex> lock(mtx);
        musica_parou = false;
        eliminados.clear();
        cadeiras = players.size() - 1;
        sem_destroy(&semaphore);
        sem_init(&semaphore, 0, cadeiras);

        // Modificação na exibição da rodada
        if (primeira_rodada) {
            cout << "\nIniciando rodada com " << players.size()
                 << " jogadores e " << cadeiras << " cadeiras.\n";
            primeira_rodada = false;  // Garantir que "Iniciando rodada" só apareça na primeira rodada
        } else {
            if (cadeiras != 1){
            cout << "\nProxima rodada com " << players.size()
                 << " jogadores e " << cadeiras << " cadeiras.\n";}
            
                 else {
                    cout << "\nProxima rodada com " << players.size()
                    << " jogadores e " << cadeiras << " cadeira.\n";
                 }
        }

        cout << "A musica esta tocando...\n";
    }

    void parar_musica() {
        unique_lock<mutex> lock(mtx);
        musica_parou = true;
        music_cv.notify_all();
    }

    void eliminar_jogador(const string& id) {
        auto it = find(players.begin(), players.end(), id);
        if (it != players.end()) {
            eliminados.push_back(id);
            players.erase(it);
        }
    }

    void exibir_estado(const vector<string>& ocupadas) {
        cout << "\n-----------------------------------------------\n";
        for (size_t i = 0; i < ocupadas.size(); ++i) {
            cout << "[Cadeira " << i + 1 << "]: Ocupada por "
                 << ocupadas[i] << "\n";
        }
        cout << "\n";
        for (auto& id : eliminados) {
            cout << "Jogador " << id
                 << " nao conseguiu uma cadeira e foi eliminado!\n";
        }
        cout << "-----------------------------------------------\n";
    }
};

class Jogador {
public:
    string id;
    JogoDasCadeiras* jogo;
    bool ativo = true;
    bool conseguiu_cadeira = false;

    Jogador(string _id, JogoDasCadeiras* _jogo)
        : id(_id), jogo(_jogo) {}

    void tentar_ocupar_cadeira() {
        unique_lock<mutex> lock(jogo->mtx);
        jogo->music_cv.wait(lock, [&] { return jogo->musica_parou; });
        lock.unlock();

        if (sem_trywait(&jogo->semaphore) == 0) {
            conseguiu_cadeira = true;
        }
    }

    void verificar_eliminacao() {
        if (!conseguiu_cadeira) {
            jogo->eliminar_jogador(id);
            ativo = false;
        }
    }
};

class Coordenador {
public:
    JogoDasCadeiras* jogo;

    Coordenador(JogoDasCadeiras* _jogo) : jogo(_jogo) {}

    void liberar_threads_eliminadas() {
        // Libera semáforo para threads que ficaram travadas
        int restantes = jogo->players.size();
        int sentados = 0;
        // Contar quantos conseguiram cadeira
        for (auto& id : jogo->players) {
            // mas jogador->conseguiu_cadeira não acessível aqui
        }
        // Simplesmente garantir que todos saiam da espera
        sem_post(&jogo->semaphore);
    }

    void iniciar_jogo(vector<Jogador*>& jogadores) {
        cout << "\n-----------------------------------------------\n";
        cout << "Bem-vindo ao Jogo das Cadeiras Concorrente!\n";
        cout << "-----------------------------------------------\n";

        while (jogo->players.size() > 1) {
            // Inicia rodada
            jogo->iniciar_rodada();

            // Embaralha a ordem dos jogadores antes de tentar ocupar as cadeiras
            random_device rd;
            mt19937 g(rd());
            shuffle(jogadores.begin(), jogadores.end(), g);  // Embaralha a ordem dos jogadores

            // Música tocando aleatório
            uniform_int_distribution<> dis(1, 3);
            this_thread::sleep_for(chrono::seconds(dis(g)));

            // Para música
            cout << "\n> A musica parou! Os jogadores estao tentando se sentar...\n";
            jogo->parar_musica();

            // Threads de jogadores tentam ocupar
            vector<thread> threads;
            for (auto& jogador : jogadores) {
                if (jogador->ativo) {
                    jogador->conseguiu_cadeira = false;
                    threads.emplace_back(&Jogador::tentar_ocupar_cadeira, jogador);
                }
            }
            for (auto& t : threads) t.join();

            // Verificar eliminações e liberar semáforo extra
            for (auto& jogador : jogadores) {
                if (jogador->ativo) {
                    jogador->verificar_eliminacao();
                }
            }

            // Montar lista de ocupantes
            vector<string> ocupadas;
            for (auto& jogador : jogadores) {
                if (jogador->ativo && jogador->conseguiu_cadeira) {
                    ocupadas.push_back(jogador->id);
                }
            }

            // Exibir estado e eliminações
            jogo->exibir_estado(ocupadas);
        }

        // Vencedor
        cout << "\nVencedor " << jogo->players.front() << "! Parabens!\n";
        cout << "-----------------------------------------------\n";
        cout << "\nObrigado por jogar o Jogo das Cadeiras Concorrente!\n\n";
    }
};

int main() {
    int n;
    cout << "\nQuantos jogadores? ";
    cin >> n;

    JogoDasCadeiras jogo(n);
    vector<Jogador*> jogadores;
    for (const auto& id : jogo.players) {
        jogadores.push_back(new Jogador(id, &jogo));
    }

    Coordenador coord(&jogo);
    coord.iniciar_jogo(jogadores);

    // Cleanup
    for (auto j : jogadores) delete j;

    return 0;
}
