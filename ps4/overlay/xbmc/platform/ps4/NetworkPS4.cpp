/*
 * Kodi POSIX network provider for OpenOrbis.
 * FreeBSD's AF_LINK/sockaddr_dl interface, used by Kodi's FreeBSD provider,
 * is not exposed by the OpenOrbis headers. The PS4 provider uses the POSIX
 * interface/socket APIs and leaves the hardware address empty when the SDK
 * cannot report it.
 */
#include "platform/posix/network/NetworkPosix.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <resolv.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

namespace
{
class CNetworkInterfacePS4 final : public CNetworkInterfacePosix
{
public:
  CNetworkInterfacePS4(CNetworkPosix* network, std::string name)
    : CNetworkInterfacePosix(network, std::move(name), EmptyMac())
  {
  }

  const std::string& InterfaceName() const { return m_interfaceName; }
  bool GetHostMacAddress(unsigned long, std::string& mac) const override
  {
    mac.clear();
    return false;
  }
  std::string GetCurrentDefaultGateway() const override { return {}; }

private:
  static char* EmptyMac()
  {
    static char mac[6]{};
    return mac;
  }
};
} // namespace

class CNetworkPS4 final : public CNetworkPosix
{
public:
  CNetworkPS4() { queryInterfaceList(); }

  std::vector<std::string> GetNameServers() override
  {
    std::vector<std::string> result;
    res_init();
    for (int i = 0; i < _res.nscount; ++i)
      result.emplace_back(inet_ntoa(_res.nsaddr_list[i].sin_addr));
    return result;
  }

  bool PingHost(unsigned long, unsigned int) override { return false; }

private:
  void GetMacAddress(const std::string&, char mac[6]) override { std::memset(mac, 0, 6); }

  void queryInterfaceList() override
  {
    for (CNetworkInterface* interface : m_interfaces)
      delete interface;
    m_interfaces.clear();

    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0)
      return;

    std::vector<std::string> addedNames;
    for (ifaddrs* current = interfaces; current; current = current->ifa_next)
    {
      if (!current->ifa_name || !current->ifa_addr || current->ifa_addr->sa_family != AF_INET)
        continue;

      const std::string name(current->ifa_name);
      if (std::find(addedNames.begin(), addedNames.end(), name) == addedNames.end())
      {
        m_interfaces.push_back(new CNetworkInterfacePS4(this, name));
        addedNames.push_back(name);
      }
    }

    freeifaddrs(interfaces);
  }
};

std::unique_ptr<CNetworkBase> CNetworkBase::GetNetwork()
{
  return std::make_unique<CNetworkPS4>();
}
