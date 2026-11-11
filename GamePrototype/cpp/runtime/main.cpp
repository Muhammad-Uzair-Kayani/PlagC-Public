#include "transport.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <survival/simulation.hpp>
using namespace survival;
using survival::client::Json;
namespace {
void print(const Snapshot &s) {
    auto rows = s.map;
    for (auto &o : s.objects)
        if (o.available)
            rows[o.position.z][o.position.x] = o.type == "plate" ? '=' : '*';
    rows[5][9] = s.gateOpen ? '/' : 'G';
    rows[s.goal.z][s.goal.x] = 'X';
    for (auto &a : s.actors)
        if (a.alive && !a.arrived)
            rows[a.position.z][a.position.x] = a.id <= 5 ? char('0' + a.id) : 'B';
    std::cout << "\nTick " << s.tick << "/" << s.maxTicks << " gate "
              << (s.gateOpen ? "open" : "closed") << "\n";
    for (auto &r : rows)
        std::cout << r << '\n';
    for (auto &a : s.actors)
        std::cout << a.id << ' ' << a.role << " HP=" << a.health << " ammo=" << a.ammo
                  << (a.arrived  ? " ARRIVED"
                      : !a.alive ? " DEAD"
                                 : "")
                  << '\n';
}
Json eventJson(const Event &e) {
    return {{"id", e.id},       {"tick", e.tick},
            {"actor", e.actor}, {"target", e.target},
            {"kind", e.kind},   {"position", {e.position.x, e.position.y, e.position.z}}};
}
Json resultJson(const Snapshot &s, int failures) {
    Json v = Json::array();
    double fitness = 0;
    for (auto &r : results(s)) {
        fitness += r.fitness;
        v.push_back({{"id", r.agent},
                     {"arrived", r.arrived},
                     {"alive", r.alive},
                     {"ticks", r.ticks},
                     {"health", r.health},
                     {"damage", r.damage},
                     {"first_attacks", r.firstAttacks},
                     {"path_cost", r.pathCost},
                     {"pickups", r.pickups},
                     {"fitness", r.fitness}});
    }
    return {{"seed", s.seed},          {"tick", s.tick},
            {"gate_open", s.gateOpen}, {"policy_failures", failures},
            {"fitness", fitness / 5},  {"agents", v}};
}
Scenario scenario(Json j) {
    Scenario s;
    s.seed = j.value("seed", 1u);
    s.maxTicks = j.value("max_ticks", 160);
    s.name = j.value("scenario", std::string("peaceful"));
    if (s.name != "peaceful" && s.name != "hostile" && s.name != "distractors" &&
        s.name != "cooperative")
        throw std::invalid_argument("unknown scenario");
    s.aggressor = s.name == "hostile" || s.name == "distractors";
    s.runner = s.name == "distractors";
    s.scavenger = s.name == "distractors";
    return s;
}
Json run(const Scenario &sc, const std::string &mode, client::PythonTransport *transport,
         int episode, const std::string &logdir) {
    if (mode != "rule" && mode != "random" && mode != "neural" && mode != "neural-no-social" &&
        mode != "cooperate")
        throw std::invalid_argument("unknown controller");
    SyntheticWorld w(sc);
    Simulation sim(w);
    std::mt19937 random(sc.seed);
    int failures = 0;
    std::ofstream trace, events;
    if (!logdir.empty()) {
        std::filesystem::create_directories(logdir);
        trace.open(logdir + "/trace.jsonl");
        events.open(logdir + "/events.jsonl");
    }
    while (!finished(w.snapshot())) {
        auto obs = sim.observations();
        std::vector<AgentIntent> intents;
        auto s = w.snapshot();
        if (mode == "neural-no-social")
            for (auto &o : obs)
                for (int k : {4, 5, 14, 15})
                    o.features[k] = 0;
        if ((mode == "neural" || mode == "neural-no-social") && !transport)
            ++failures;
        if ((mode == "neural" || mode == "neural-no-social") && transport) {
            try {
                intents = transport->infer(obs, std::uint64_t(s.tick) + 1, episode, s.tick);
            } catch (const std::exception &e) {
                ++failures;
                if (failures == 1)
                    std::cerr << "Policy fallback: " << e.what() << '\n';
            }
        }
        if (mode == "random" || mode == "cooperate")
            for (auto &o : obs) {
                std::vector<Intent> available;
                for (int k = 0; k < 6; ++k)
                    if (o.valid[k])
                        available.push_back(static_cast<Intent>(k));
                auto i = mode == "random" ? available[random() % available.size()]
                                          : (o.valid[2] ? Intent::Cooperate : Intent::Progress);
                intents.push_back({o.agent, i, 0});
            }
        if (trace)
            for (auto &o : obs) {
                auto it = std::find_if(intents.begin(), intents.end(),
                                       [&](auto &i) { return i.agent == o.agent; });
                auto i = it == intents.end() ? baseline(s, o, sim.memories[o.agent]) : *it;
                trace << Json({{"seed", s.seed},
                               {"tick", s.tick},
                               {"observation", client::encode(o)},
                               {"intent", name(i.intent)}})
                             .dump()
                      << '\n';
            }
        sim.step(intents);
        if (events)
            for (auto &e : w.snapshot().events)
                events << eventJson(e).dump() << '\n';
    }
    auto r = resultJson(w.snapshot(), failures);
    if (trace) {
        std::ofstream out(logdir + "/result.json");
        out << r.dump(2);
    }
    return r;
}
} // namespace
int main(int argc, char **argv) {
    try {
        Json options = Json::object();
        bool server = false, headless = false;
        std::string service, model, python = "python3", mode = "rule", logs;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            auto value = [&]() {
                if (++i >= argc)
                    throw std::invalid_argument("missing argument for " + a);
                return std::string(argv[i]);
            };
            if (a == "--server")
                server = true;
            else if (a == "--headless")
                headless = true;
            else if (a == "--seed")
                options["seed"] = std::stoul(value());
            else if (a == "--max-ticks")
                options["max_ticks"] = std::stoi(value());
            else if (a == "--scenario")
                options["scenario"] = value();
            else if (a == "--controller")
                mode = value();
            else if (a == "--policy-service")
                service = value();
            else if (a == "--model")
                model = value();
            else if (a == "--python")
                python = value();
            else if (a == "--log-dir")
                logs = value();
            else if (a == "--help") {
                std::cout
                    << "GameRuntime [--headless|--server] [--seed N] [--scenario "
                       "peaceful|hostile|distractors|cooperative] [--controller "
                       "rule|random|cooperate|neural] [--policy-service python/service.py] "
                       "[--model FILE] [--python EXECUTABLE] [--log-dir DIR] [--max-ticks N]\n";
                return 0;
            } else
                throw std::invalid_argument("unknown argument: " + a);
        }
        std::unique_ptr<client::PythonTransport> transport;
        if (!service.empty()) {
            try {
                transport = std::make_unique<client::PythonTransport>(python, service);
                if (!model.empty())
                    transport->request({{"v", 1},
                                        {"type", "load_policy"},
                                        {"request_id", 0},
                                        {"episode", 0},
                                        {"path", model}});
            } catch (const std::exception &e) {
                std::cerr << "Python unavailable; using rule fallback: " << e.what() << '\n';
                transport.reset();
            }
        }
        if (server) {
            std::string line;
            while (std::getline(std::cin, line)) {
                Json q;
                try {
                    q = Json::parse(line);
                    if (q.at("v") != 1)
                        throw std::invalid_argument("schema version");
                    auto type = q.at("type").get<std::string>();
                    Json response{{"v", 1},
                                  {"request_id", q.at("request_id")},
                                  {"episode", q.value("episode", 0)}};
                    if (type == "shutdown") {
                        response["type"] = "bye";
                        std::cout << response.dump() << std::endl;
                        break;
                    } else if (type == "hello")
                        response["type"] = "ready";
                    else if (type == "evaluate_genome") {
                        auto controller = q.value("controller", std::string("neural"));
                        if ((controller == "neural" || controller == "neural-no-social") &&
                            !transport)
                            throw std::runtime_error(
                                "neural evaluation requires working --policy-service");
                        if (q.contains("weights")) {
                            if (!transport)
                                throw std::runtime_error("no policy transport");
                            transport->request({{"v", 1},
                                                {"type", "load_policy"},
                                                {"request_id", 0},
                                                {"episode", 0},
                                                {"weights", q.at("weights")}});
                        }
                        response["type"] = "episode_result";
                        response["result"] =
                            run(scenario(q), controller, transport.get(), q.value("episode", 0),
                                q.value("log_dir", std::string{}));
                    } else
                        throw std::invalid_argument("unknown command");
                    std::cout << response.dump() << std::endl;
                } catch (const std::exception &e) {
                    std::cout << Json({{"v", 1},
                                       {"type", "error"},
                                       {"request_id",
                                        q.is_object() ? q.value("request_id", Json{}) : Json{}},
                                       {"error", e.what()}})
                                     .dump()
                              << std::endl;
                }
            }
            return 0;
        }
        if (headless) {
            std::cout << run(scenario(options), mode, transport.get(), 0, logs).dump() << '\n';
            return 0;
        }
        SyntheticWorld w(scenario(options));
        Simulation sim(w);
        print(w.snapshot());
        std::cout
            << "1 is you; 2-5 are independent AI. WASD move; E interact; F <id> fire; I inspect; M "
               "map; L events; P autopilot tick; Enter/Space wait; R reset; Q quit.\n";
        std::string line;
        while (std::cout << "> " && std::getline(std::cin, line)) {
            char key = line.empty() ? ' ' : char(std::tolower(static_cast<unsigned char>(line[0])));
            if (key == 'q')
                break;
            if (key == 'r') {
                w.reset(scenario(options));
                sim.memories.clear();
                print(w.snapshot());
                continue;
            }
            auto s = w.snapshot();
            if (key == 'm') {
                print(s);
                continue;
            }
            if (key == 'l') {
                for (auto &e : s.events)
                    std::cout << eventJson(e).dump() << '\n';
                continue;
            }
            if (key == 'i') {
                auto o = observe(s, 1, sim.memories[1]);
                std::cout << client::encode(o).dump(2)
                          << "\nRemembered actors: " << sim.memories[1].beliefs.size() << '\n';
                continue;
            }
            if (finished(s)) {
                std::cout << resultJson(s, 0).dump(2) << "\nR resets.\n";
                continue;
            }
            Command human;
            human.actor = 1;
            if (key == 'w')
                human.move = {0, 0, -1};
            else if (key == 's')
                human.move = {0, 0, 1};
            else if (key == 'a')
                human.move = {-1, 0, 0};
            else if (key == 'd')
                human.move = {1, 0, 0};
            else if (key == 'e')
                human.interact = true;
            else if (key == 'f') {
                try {
                    human.target = std::stoul(line.substr(1));
                    human.fire = true;
                } catch (...) {
                    std::cout << "Use F followed by a visible actor ID.\n";
                    continue;
                }
            } else if (key != ' ' && key != 'p') {
                std::cout << "Unknown command; use --help for startup options.\n";
                continue;
            }
            auto obs = sim.observations();
            std::vector<AgentIntent> intents;
            if (transport) {
                try {
                    intents = transport->infer(obs, s.tick + 1, 0, s.tick);
                } catch (const std::exception &e) {
                    std::cerr << e.what() << "; rule fallback\n";
                }
            }
            sim.step(intents, key == 'p' ? nullptr : &human);
            print(w.snapshot());
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
