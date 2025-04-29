#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <chrono>
#include <atomic>

class MusicalChairs {
private:
    int total_players;
    std::atomic<int> chairs_available;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> music_playing;
    std::vector<std::atomic<bool>> players_active;
    bool round_in_progress;

    std::random_device rd;
    std::mt19937 gen;

public:
    MusicalChairs(int players) : 
        total_players(players),
        chairs_available(0),
        music_playing(false),
        round_in_progress(false),
        gen(rd()),
        players_active(players) 
    {
        for(size_t i = 0; i < players_active.size(); ++i) {
            players_active[i].store(true);
        }
    }

    void play_round() {
        // Configura nova rodada
        int active = count_active_players();
        chairs_available = active - 1;
        round_in_progress = true;

        {
            std::lock_guard<std::mutex> lock(mtx);
            music_playing = true;
            std::cout << "\nMusica começou!\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds(2 + (gen() % 2)));

        {
            std::lock_guard<std::mutex> lock(mtx);
            music_playing = false;
            std::cout << "Musica parou! Corram para as cadeiras!\n";
            cv.notify_all();
        }

        // Espera todos os jogadores processarem
        while(chairs_available > 0 && count_active_players() > 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        round_in_progress = false;
    }

    bool try_get_chair(int player_id) {
        std::unique_lock<std::mutex> lock(mtx);
        
        cv.wait(lock, [this] { return !music_playing.load(); });

        if(!players_active[player_id].load() || !round_in_progress) return false;

        if(chairs_available > 0) {
            chairs_available--;
            std::cout << "Jogador " << player_id << " conseguiu uma cadeira!\n";
            return true;
        }
        else {
            if(players_active[player_id].exchange(false)) {
                std::cout << "Jogador " << player_id << " foi eliminado!\n";
            }
            return false;
        }
    }

    void player_thread(int player_id) {
        while(players_active[player_id].load()) {
            try_get_chair(player_id);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void start_game() {
        std::vector<std::thread> players;
        for(int i = 0; i < total_players; ++i) {
            players.emplace_back(&MusicalChairs::player_thread, this, i);
        }

        while(count_active_players() > 1) {
            play_round();
            
            std::cout << "\n--- Status ---\n"
                      << "Jogadores restantes: " << count_active_players() << "\n"
                      << "Proxima rodada: " << (count_active_players() - 1) << " cadeiras\n\n";
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        for(auto& t : players) {
            if(t.joinable()) t.join();
        }

        int winner_index = -1;
        for(size_t i = 0; i < players_active.size(); ++i) {
            if(players_active[i].load()) {
                winner_index = i;
                break;
            }
        }

        if(winner_index != -1) {
            std::cout << "\nVencedor: Jogador " << winner_index << "\n";
        }
    }

private:
    int count_active_players() {
        int count = 0;
        for(auto& p : players_active) {
            if(p.load()) count++;
        }
        return count;
    }
};

int main() {
    int n;
    std::cout << "Quantos jogadores? ";
    std::cin >> n;

    MusicalChairs(n).start_game();
    return 0;
}