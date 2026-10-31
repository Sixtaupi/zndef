Build the documentation
=======================

Python 3.13 or newer and Doxygen 1.9.8 or newer are required. No Zephyr SDK is
needed to build this standalone documentation. The Sphinx configuration and
Makefile are adapted from the Zephyr example application.

From the repository root:

.. code-block:: sh

   python3 -m venv /tmp/zndef-doc-venv
   . /tmp/zndef-doc-venv/bin/activate
   python -m pip install --require-hashes -r doc/requirements.txt
   python -m sphinx -M html doc build-doc -W --keep-going

Open ``build-doc/html/index.html``. Alternatively, use the supplied template:

.. code-block:: sh

   make -C doc html SPHINXOPTS="-W --keep-going"

This writes HTML to ``doc/_build_sphinx/html``. ``doc/make.bat html`` provides
the equivalent Windows entry point. Both builds automatically generate Doxygen
XML before rendering the API with Breathe. The Zephyr intersphinx inventory
requires network access on the first build; Sphinx caches it for later builds.

To generate standalone Doxygen HTML and XML, run from the repository root:

.. code-block:: sh

   doxygen doc/Doxyfile

Open ``build-doc/doxygen/html/index.html``.

The documentation has two parts: ``introduction.rst`` and ``api/index.rst``.
API pages describe usage before rendering functions, followed by structures,
typedefs, enums and macros. The small extension in ``_extensions/ndef_api.py``
uses Breathe's section ordering for this layout. API contracts remain in the
public headers; editing them updates the generated reference on the next build.

CI and publication
------------------

The ``Documentation`` workflow in ``.github/workflows/documentation.yml`` builds the
reference on pushes to ``master`` and on pull requests. Doxygen and Sphinx
warnings fail the build. Python dependencies are installed from the
hash-locked ``doc/requirements.txt`` with Python 3.13.

Every successful build uploads the HTML as the ``github-pages`` artifact,
retained for 14 days. Pull requests only build and upload; publication runs
only on pushes to ``master`` after a successful build.

To enable hosting once, open the repository's **Settings > Pages**, then set
**Build and deployment > Source** to **GitHub Actions**. Push the workflow to
``master`` to trigger generation and publication. The deployment URL appears in the ``github-pages`` environment and the workflow summary.
No personal access token or separate generated-content branch is needed.

See the `GitHub Pages custom workflow documentation
<https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages>`_
for repository hosting requirements and settings.
