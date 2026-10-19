# =============================================================================
# Benchmark des allocateurs mémoire
# =============================================================================
# Cibles principales :
#   make                 build + run tous les allocs + génère report.html
#   make check-deps      vérifie la présence des libs externes
#   make list            affiche les flags de link de chaque allocateur
#   make run-hajlibc     relance UNIQUEMENT hajlibc (archive l'ancien run)
#   make report-hajlibc  run-hajlibc + report
#   make fclean          nettoie tout (bin/, results/, report.html)
#   make clean-old       supprime l'archive hajlibc_old
#
# NOTE importante :
#   - hajlibc et mimalloc : lancés sur le host (glibc native).
#   - tcmalloc / jemalloc : les .so viennent du rootfs junest et requièrent
#     une glibc plus récente que celle du host. On les lance donc DANS junest
#     (même rootfs que les .so), via LD_PRELOAD, sur le binaire bench_tcmalloc
#     compilé normalement (non linké aux libs externes).
# =============================================================================

CC       ?= clang
CFLAGS   ?= -O2 -Wall -Wextra -pthread -g -D_FORTIFY_SOURCE=0 \
            -fno-builtin-malloc -fno-builtin-free \
            -fno-builtin-realloc -fno-builtin-calloc
LDFLAGS  ?= -pthread

SRC      := bench.c
BINDIR   := bin
RESDIR   := results
PY       ?= python3

# Commande junest (adapte si tu utilises un alias/chemin différent).
JUNEST   ?= junest

# Chemins des .so externes (vus depuis l'intérieur de junest).
HAJ_LIB      := ../../libft_malloc.so
JUNEST_LIB   := /sgoinfre/goinfre/Perso/ele-lean/.junest/usr/lib
MIMALLOC_LIB := /home/ele-lean/mimalloc-v1.15.2-linux-x64/lib

MIMALLOC_SO  := $(MIMALLOC_LIB)/libmimalloc.so
TCMALLOC_SO  := $(JUNEST_LIB)/libtcmalloc_minimal.so
JEMALLOC_SO  := $(JUNEST_LIB)/libjemalloc.so

# Liste des allocateurs testés.
ALLOC_NAMES := glibc hajlibc mimalloc tcmalloc jemalloc

# -----------------------------------------------------------------------------
# Flags de link :
#   - glibc                : rien
#   - mimalloc             : linké normalement (install locale compatible host)
#   - hajlibc/tcmalloc/jemalloc : LD_PRELOAD au runtime, pas de flag
# -----------------------------------------------------------------------------
ALLOC_FLAGS_glibc    :=
ALLOC_FLAGS_hajlibc  :=
ALLOC_FLAGS_mimalloc := -L$(MIMALLOC_LIB) -lmimalloc -Wl,-rpath,$(MIMALLOC_LIB)
ALLOC_FLAGS_tcmalloc :=
ALLOC_FLAGS_jemalloc :=

# -----------------------------------------------------------------------------
# Détection de la présence des libs externes.
# Une seule commande shell (pas de if/then/fi multiligne).
# -----------------------------------------------------------------------------
define is_buildable
$(shell if [ "$(1)" = "hajlibc" ]; then test -f "$(HAJ_LIB)" && echo 1 || echo 0; elif [ "$(1)" = "mimalloc" ]; then test -e "$(MIMALLOC_SO)" && echo 1 || echo 0; elif [ "$(1)" = "tcmalloc" ]; then test -e "$(TCMALLOC_SO)" && echo 1 || echo 0; elif [ "$(1)" = "jemalloc" ]; then test -e "$(JEMALLOC_SO)" && echo 1 || echo 0; else echo 1; fi)
endef

BUILDABLE := $(foreach name,$(ALLOC_NAMES), \
    $(if $(filter 1,$(call is_buildable,$(name))), \
        $(BINDIR)/bench_$(name)))

# =============================================================================
# Cibles principales
# =============================================================================
all: check-deps $(BUILDABLE) run report

build: $(BUILDABLE)

# -----------------------------------------------------------------------------
# check-deps : diagnostic des libs disponibles
# -----------------------------------------------------------------------------
check-deps:
	@echo "--- Détection des allocateurs ---"
	@$(foreach name,$(ALLOC_NAMES), \
		if [ "$(name)" = "hajlibc" ]; then \
			if [ -f "$(HAJ_LIB)" ]; then \
				echo "  hajlibc    : found ($(HAJ_LIB)) [LD_PRELOAD, host]"; \
			else \
				echo "  hajlibc    : NOT BUILT (run 'make malloc' at repo root)"; \
			fi; \
		elif [ "$(name)" = "mimalloc" ]; then \
			if [ -e "$(MIMALLOC_SO)" ]; then \
				echo "  mimalloc   : found ($(MIMALLOC_SO)) [linked, host]"; \
			else \
				echo "  mimalloc   : NOT FOUND ($(MIMALLOC_SO))"; \
			fi; \
		elif [ "$(name)" = "tcmalloc" ]; then \
			if [ -e "$(TCMALLOC_SO)" ]; then \
				echo "  tcmalloc   : found ($(TCMALLOC_SO)) [LD_PRELOAD, junest]"; \
			else \
				echo "  tcmalloc   : NOT FOUND ($(TCMALLOC_SO))"; \
			fi; \
		elif [ "$(name)" = "jemalloc" ]; then \
			if [ -e "$(JEMALLOC_SO)" ]; then \
				echo "  jemalloc   : found ($(JEMALLOC_SO)) [LD_PRELOAD, junest]"; \
			else \
				echo "  jemalloc   : NOT FOUND ($(JEMALLOC_SO))"; \
			fi; \
		else \
			echo "  $(name)     : Always available (glibc)"; \
		fi;)
	@echo "---------------------------------"

# -----------------------------------------------------------------------------
# list : affiche les flags de link
# -----------------------------------------------------------------------------
list:
	@echo "Allocateurs configurés :"
	@$(foreach name,$(ALLOC_NAMES), \
		flags="$(ALLOC_FLAGS_$(name))"; \
		if [ -z "$$flags" ]; then \
			printf "  - %-12s ->  (LD_PRELOAD or default)\n" "$(name)"; \
		else \
			printf "  - %-12s ->  %s\n" "$(name)" "$$flags"; \
		fi;)

# =============================================================================
# Build des binaires
# =============================================================================
$(BINDIR):
	mkdir -p $(BINDIR)

$(RESDIR):
	mkdir -p $(RESDIR)

define ALLOC_RULE
$(BINDIR)/bench_$(1): $(SRC) | $(BINDIR)
	$$(CC) $$(CFLAGS) -o $$@ $$(SRC) $$(LDFLAGS) $(ALLOC_FLAGS_$(1))
endef

$(foreach name,$(ALLOC_NAMES), \
	$(eval $(call ALLOC_RULE,$(name))))

# =============================================================================
# Backup hajlibc -> hajlibc_old
# =============================================================================
backup-hajlibc: | $(RESDIR)
	@if [ -f "$(RESDIR)/results_hajlibc.csv" ]; then \
		mv -f "$(RESDIR)/results_hajlibc.csv" "$(RESDIR)/results_hajlibc_old.csv"; \
		echo "  ↻ hajlibc : ancien run archivé -> results_hajlibc_old.csv"; \
	fi
	@if [ -f "$(RESDIR)/timeseries_hajlibc.csv" ]; then \
		mv -f "$(RESDIR)/timeseries_hajlibc.csv" "$(RESDIR)/timeseries_hajlibc_old.csv"; \
	fi

# =============================================================================
# Run : tous les allocateurs
#   - glibc, hajlibc, mimalloc : host
#   - tcmalloc, jemalloc       : dans junest (glibc compatible avec leurs .so)
# NB : bench.c écrit chaque ligne de CSV + fflush immédiatement, donc si un
# allocateur crash, les tests déjà effectués restent exploitables. On conserve
# donc les CSV partiels et on se contente d'un avertissement.
# =============================================================================
run: backup-hajlibc | $(RESDIR)
	@mkdir -p $(RESDIR)
	@for bin in $(BUILDABLE); do \
		name=$$(basename $$bin | sed 's/bench_//'); \
		echo ""; \
		echo ">>> Launching benchmark: $$name"; \
		case "$$name" in \
			hajlibc)  LD_PRELOAD="$(HAJ_LIB)"     ./$$bin $$name $(RESDIR) ;; \
			tcmalloc) $(JUNEST) -- env LD_PRELOAD="$(TCMALLOC_SO)" ./$$bin $$name $(RESDIR) ;; \
			jemalloc) $(JUNEST) -- env LD_PRELOAD="$(JEMALLOC_SO)" ./$$bin $$name $(RESDIR) ;; \
			*)        ./$$bin $$name $(RESDIR) ;; \
		esac; \
		rc=$$?; \
		if [ $$rc -ne 0 ]; then \
			lines=$$(wc -l < "$(RESDIR)/results_$$name.csv" 2>/dev/null || echo 0); \
			echo "  ⚠️  $$name a crashé (code $$rc) — $$lines ligne(s) conservées dans $(RESDIR)/results_$$name.csv"; \
		fi; \
	done
	@echo ""
	@echo "Results are in: $(RESDIR)/"

# =============================================================================
# Run : hajlibc uniquement (itération rapide)
# =============================================================================
run-hajlibc: backup-hajlibc $(BINDIR)/bench_hajlibc | $(RESDIR)
	@echo ""
	@echo ">>> Launching benchmark: hajlibc (only)"
	@LD_PRELOAD=$(HAJ_LIB) ./$(BINDIR)/bench_hajlibc hajlibc $(RESDIR)
	@echo ""
	@echo "hajlibc results refreshed in: $(RESDIR)/"

report-hajlibc: run-hajlibc report

# =============================================================================
# Rapport HTML
# =============================================================================
report:
	$(PY) analyze.py --results-dir $(RESDIR) --output report.html
	@echo "Report generated: report.html"

# =============================================================================
# Nettoyage
# =============================================================================
clean:
	@rm -f $(BINDIR)/* $(RESDIR)/*

fclean: clean
	@rm -rf $(BINDIR) $(RESDIR) report.html scores.json

clean-old:
	@rm -f $(RESDIR)/results_hajlibc_old.csv \
	       $(RESDIR)/timeseries_hajlibc_old.csv
	@echo "Archived hajlibc results removed."

.PHONY: all build check-deps list run run-hajlibc report report-hajlibc \
        backup-hajlibc clean fclean clean-old
