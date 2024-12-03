IFACE ?= lo
PROG ?= xdp_ddos_protection
MAP ?= rate_limit_map

.PHONY: all compile clean attach detach dump

all: detach clean compile attach dump

compile: clean
	clang -O2 -g -target bpf -c $(PROG).c -o $(PROG).o

clean:
	rm -f xdp_ddos_protection.o

attach:
	sudo ip link set dev $(IFACE) xdp obj $(PROG).o sec xdp

detach:
	sudo ip link set dev $(IFACE) xdp off

iface-inspect:
	sudo ip link show $(IFACE)

dump:
	sudo bpftool map dump name $(MAP) &> /dev/null
