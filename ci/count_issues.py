import requests, os, sys, json

API_URL = os.environ.get("CI_API_V4_URL")
TOKEN = os.environ.get("PROJECT_TOKEN")
PROJECT_ID = os.environ.get("CI_PROJECT_ID")
PROJECT_PATH = os.environ.get("CI_PROJECT_PATH")

start = sys.argv[1]
end = sys.argv[2]

def count_issues():
    try:
        headers = {
            'PRIVATE-TOKEN': TOKEN,
        }
        url = f'{API_URL}/projects/{PROJECT_ID}/issues'
        params = {
            'page': 1,  # Start with page 3
            'per_page': 100,  # Set the number of jobs to retrieve per page (adjust as needed)
        }

        count = {
            "closed_this_month": {},
            "opened_this_month": {},
            "closed": {},
            "open": {}
        }
        while True:
            response = requests.get(url, headers=headers, params=params, verify=False)
            issues = response.json()

            for issue in issues:
                issueType = "unknown"
                if "bug" in issue["labels"]:
                    issueType = "bug"
                elif any(label in ["feature", "enhancement"] for label in issue["labels"]):
                    issueType = "feature"
                elif any(label in ["support", "documentation", "Testing"] for label in issue["labels"]):
                    issueType = "support"
                elif any(label in ["NFR", "discussion"] for label in issue["labels"]):
                    issueType = "nfr"
                elif "duplicate" in issue["labels"]:
                    issueType = "duplicate"
                else:
                    print(f'Label needed for {PROJECT_PATH}/issues/{issue["iid"]}!!')

                if issue["closed_at"] and issue["closed_at"] > start and issue["closed_at"] < end:
                    count["closed_this_month"][issueType] = count["closed_this_month"].get(issueType, 0) + 1
                elif issue["created_at"] and issue["created_at"] > start and issue["created_at"] < end:
                    count["opened_this_month"][issueType] = count["opened_this_month"].get(issueType, 0) + 1

                if issue["state"] == "closed":
                    count["closed"][issueType] = count["closed"].get(issueType, 0) + 1
                elif issue["state"] == "opened":
                    count["open"][issueType] = count["open"].get(issueType, 0) + 1

            # Check if there are more pages to retrieve
            if 'next' in response.links:
                url = response.links['next']['url']
                params = None  # Clear the params to follow the next_page URL
            else:
                break
        
        output = json.dumps(count, indent=2)
        print(output)

    except requests.exceptions.RequestException as error:
        print('Error retrieving GitLab issues:', error)

count_issues()
