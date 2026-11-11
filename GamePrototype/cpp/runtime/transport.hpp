#pragma once
#include <memory>
#include <nlohmann/json.hpp>
#include <survival/simulation.hpp>
namespace survival::client {
using Json = nlohmann::json;
Json encode(const Observation &);
class PythonTransport final : public IAITransport {
  public:
    PythonTransport(const std::string &executable, const std::string &service,
                    int timeoutMs = 2000);
    ~PythonTransport();
    Json request(Json);
    std::vector<AgentIntent> infer(const std::vector<Observation> &, std::uint64_t, int,
                                   int) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace survival::client
