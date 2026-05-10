default:
	@just --list

list:
	@just --list

generate:
	python3 scripts/tasks.py generate

build:
	python3 scripts/tasks.py build

test-cpp:
	python3 scripts/tasks.py test-cpp

test-c-ffi:
	python3 scripts/tasks.py test-c-ffi

test-py-ffi:
	python3 scripts/tasks.py test-py-ffi

test: cross-all
	python3 scripts/tasks.py test-cpp
	python3 scripts/tasks.py test-c-ffi
	python3 scripts/tasks.py test-py-ffi

run: generate
	python3 scripts/tasks.py run

cross target:
	python3 scripts/tasks.py cross {{target}}

cross-all: generate
	python3 scripts/tasks.py cross-all

install libdir="/usr/local/lib":
	python3 scripts/tasks.py install {{libdir}}

clean:
	python3 scripts/tasks.py clean
