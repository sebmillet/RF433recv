ALL:

clean:
	make -C examples/01_generic/ $@
	make -C examples/02_recv/ $@
	make -C examples/03_example_of_readme/ $@
	make -C examples/04_rcswitch_recv/ $@
	make -C examples/05_rollingcode/ $@
	make -C examples/06_recv_20220829083700/ $@
	make -C extras/testplan/test/ $@

mrproper:
	make -C examples/01_generic/ $@
	make -C examples/02_recv/ $@
	make -C examples/03_example_of_readme/ $@
	make -C examples/04_rcswitch_recv/ $@
	make -C examples/05_rollingcode/ $@
	make -C examples/06_recv_20220829083700/ $@
	make -C extras/testplan/test/ $@

