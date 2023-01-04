
counter = counter and counter + 1 or 0
pd.post(string.format("loadtest: counter = %d", counter))
