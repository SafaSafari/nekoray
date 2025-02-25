package main

import (
	"encoding/binary"
	"log"
	"neko/pkg/iphlpapi"
	"net"
	"strings"
	"sync"
	"syscall"
	"unsafe"

	"github.com/v2fly/v2ray-core/v5/transport/internet"
)

// https://docs.microsoft.com/en-us/windows/win32/api/ipmib/ns-ipmib-mib_ipforwardrow
var routes []iphlpapi.RouteRow
var interfaces []net.Interface
var lock sync.Mutex

func init() {
	internet.RegisterListenerController(func(network, address string, fd uintptr) error {
		bindInterfaceIndex := getBindInterfaceIndex(address)
		if bindInterfaceIndex != 0 {
			if err := bindInterface(fd, bindInterfaceIndex, true, true); err != nil {
				log.Println("bind inbound interface", err)
				return err
			}
		}
		return nil
	})
	internet.RegisterDialerController(func(network, address string, fd uintptr) error {
		bindInterfaceIndex := getBindInterfaceIndex(address)
		if bindInterfaceIndex != 0 {
			var v4, v6 bool
			if strings.HasSuffix(network, "6") {
				v4 = false
				v6 = true
			} else {
				v4 = true
				v6 = false
			}
			if err := bindInterface(fd, bindInterfaceIndex, v4, v6); err != nil {
				log.Println("bind outbound interface", err)
				return err
			}
		}
		return nil
	})
	updateRoutes()
	iphlpapi.RegisterNotifyRouteChange2(func(callerContext uintptr, row uintptr, notificationType uint32) uintptr {
		updateRoutes()
		return 0
	}, true)
}

func updateRoutes() {
	lock.Lock()
	defer lock.Unlock()

	var err error
	routes, err = iphlpapi.GetRoutes()
	if err != nil {
		log.Println("warning: GetRoutes failed", err)
	}
	interfaces, err = net.Interfaces()
	if err != nil {
		log.Println("warning: Interfaces failed", err)
	}
}

func getBindInterfaceIndex(address string) uint32 {
	host, _, _ := net.SplitHostPort(address)
	if net.ParseIP(host).IsLoopback() {
		return 0
	}

	lock.Lock()
	defer lock.Unlock()

	if interfaces == nil {
		log.Println("warning: no interfaces info")
		return 0
	}

	var nextInterface int
	for i, intf := range interfaces {
		if intf.Name == "nekoray-tun" || intf.Name == "wintun" || intf.Name == "TunMax" {
			if len(interfaces) > i+1 {
				nextInterface = interfaces[i+1].Index
			}
			break
		}
	}

	// Not in VPN mode
	if nextInterface == 0 {
		return 0
	}

	if routes == nil {
		log.Println("warning: no routes info")
		return 0
	}

	for _, route := range routes {
		// MIB_IPROUTE_TYPE_INDIRECT
		if route.ForwardType == 4 {
			// MIB_IPPROTO_NETMGMT
			if route.ForwardProto == 3 {
				if route.GetForwardMask().IsUnspecified() {
					return route.ForwardIfIndex
				}
			}
		}
	}

	// Default route not found
	return uint32(nextInterface)
}

const (
	IP_UNICAST_IF   = 31 // nolint: golint,stylecheck
	IPV6_UNICAST_IF = 31 // nolint: golint,stylecheck
)

func bindInterface(fd uintptr, interfaceIndex uint32, v4, v6 bool) error {
	if v4 {
		/* MSDN says for IPv4 this needs to be in net byte order, so that it's like an IP address with leading zeros. */
		bytes := make([]byte, 4)
		binary.BigEndian.PutUint32(bytes, interfaceIndex)
		interfaceIndex_v4 := *(*uint32)(unsafe.Pointer(&bytes[0]))

		if err := syscall.SetsockoptInt(syscall.Handle(fd), syscall.IPPROTO_IP, IP_UNICAST_IF, int(interfaceIndex_v4)); err != nil {
			return err
		}
	}

	if v6 {
		if err := syscall.SetsockoptInt(syscall.Handle(fd), syscall.IPPROTO_IPV6, IPV6_UNICAST_IF, int(interfaceIndex)); err != nil {
			return err
		}
	}

	return nil
}
