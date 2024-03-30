define TARGET_template =
 OBJS_$(1) := $$(foreach name,$(2),build/$$(name).o)
 build/$(1): $$(OBJS_$(1))
	$(CXX) -o build/$(1) $$(OBJS_$(1)) $(LDFLAGS) $(LDFLAGS_$(1))
endef

all: $(TARGETS:%=build/%)

$(foreach target,$(TARGETS),\
	$(eval $(call TARGET_template,$(target),$(UNITS_$(target)))))

$(foreach target,$(TARGETS),\
	$(foreach unit,$(UNITS_$(target)),\
		$(eval ALLUNITS := $(filter-out $(unit),$(ALLUNITS)) $(unit))))

$(ALLUNITS:%=build/%.o): build/%.o: src/%.cc build/.dir
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(foreach dir,$(foreach unit,$(ALLUNITS),$(shell dirname build/$(unit))),\
	$(eval ALLDIRS := $(filter-out $(dir),$(ALLDIRS)) $(dir)))

build/.dir:
	mkdir -p $(ALLDIRS)
	touch build/.dir

.PHONY: clean
clean:
	rm -rf build

-include $(ALLUNITS:%=build/%.d)
