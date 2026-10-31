# Copyright (c) 2026 Romain Pelletant
# SPDX-License-Identifier: Apache-2.0

"""Expose Breathe's section ordering for API groups."""

from breathe.directives.content_block import DoxygenGroupDirective


class ApiGroupDirective(DoxygenGroupDirective):
    def run(self):
        # Breathe's renderer already supports ordered sections for compounds.
        # Include member sections so that structure fields remain documented.
        self.options["sections"] = (
            "briefdescription detaileddescription func innerclass typedef enum "
            "define user-defined var public-type public-func public-attrib"
        )
        self.options["no-title"] = True
        self.options["members"] = ""
        return super().run()


def setup(app):
    app.add_directive("doxygen-api", ApiGroupDirective)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
