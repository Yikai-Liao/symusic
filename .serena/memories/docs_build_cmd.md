Build docs locally:

```bash
python -m venv .venv-docs
source .venv-docs/bin/activate
pip install -r docs_rtd/requirements.txt
sphinx-build -b html docs_rtd docs_rtd/_build/html
```

Once built, open `docs_rtd/_build/html/index.html` to preview.