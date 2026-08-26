SHELL := /usr/bin/env bash
.DEFAULT_GOAL := help

.PHONY: help doctor bootstrap bootstrap-gbrecomp bootstrap-sgdk generate fixtures test upstream-test irq-test timer-test cake-test mbc1-test mbc3-test mbc5-test syntax-test mapper-size verify-ci verify sgdk sgdk-mbc5 clean distclean

help:
	@printf '%s\n' \
	  'gbc-to-md targets:' \
	  '  make doctor            Check host build prerequisites' \
	  '  make bootstrap         Install pinned GB Recompiled into .deps/' \
	  '  make bootstrap-sgdk    Install SGDK + m68k-elf toolchain into .deps/' \
	  '  make test              Run core backend/runtime tests and perf smoke' \
	  '  make verify-ci         Run the full host compatibility suite' \
	  '  make verify            verify-ci + generated mapper size report' \
	  '  make sgdk              Build cakegame Mega Drive ROM (requires SGDK deps)' \
	  '  make sgdk-mbc5         Build the 8 MiB far-ROM MBC5 fixture' \
	  '  make clean             Remove generated build outputs' \
	  '  make distclean         Remove build outputs and downloaded dependencies'

doctor:
	@./scripts/doctor.sh

bootstrap: bootstrap-gbrecomp
bootstrap-gbrecomp:
	@./scripts/bootstrap-gbrecomp.sh
bootstrap-sgdk:
	@./scripts/bootstrap-sgdk.sh
	@./scripts/bootstrap-toolchain.sh

generate: bootstrap-gbrecomp
	@./scripts/generate.sh "$${GAME:-cakegame}"

fixtures:
	@for f in fixtures/make_*.py; do python3 "$$f"; done

test: bootstrap-gbrecomp
	@./tests/run_core_tests.sh
upstream-test: bootstrap-gbrecomp
	@./tests/run_upstream_tests.sh
irq-test: bootstrap-gbrecomp
	@./tests/run_irq_tests.sh
timer-test: bootstrap-gbrecomp
	@./tests/run_timer_tests.sh
cake-test: bootstrap-gbrecomp
	@./tests/run_cake_tests.sh
mbc1-test: bootstrap-gbrecomp
	@./tests/run_mbc1_tests.sh
mbc3-test: bootstrap-gbrecomp
	@./tests/run_mbc3_tests.sh
mbc5-test: bootstrap-gbrecomp
	@./tests/run_mbc5_tests.sh
syntax-test: bootstrap-gbrecomp
	@./tests/run_sgdk_syntax_tests.sh

verify-ci: test upstream-test irq-test timer-test cake-test mbc1-test mbc3-test mbc5-test syntax-test
	@echo 'All CI verification targets passed.'

mapper-size: mbc1-test mbc3-test mbc5-test
	@./tests/run_mapper_size.sh

verify: verify-ci mapper-size
	@echo 'Full verification passed.'

sgdk: bootstrap-gbrecomp bootstrap-sgdk
	@GAME=cakegame SGDK_BUILD=release ./scripts/build-sgdk.sh

sgdk-mbc5: bootstrap-gbrecomp bootstrap-sgdk
	@SGDK="$$(pwd)/.deps/SGDK/$$(. ./versions.env; printf '%s' "$$SGDK_VERSION")" ./scripts/enable-sgdk-bank-switch.sh
	@GAME=mbc5test SGDK_BUILD=release ./scripts/build-sgdk.sh

clean:
	rm -rf build
	rm -f fixtures/*.gb fixtures/*.annotations fixtures/*.sha256

distclean: clean
	rm -rf .deps
