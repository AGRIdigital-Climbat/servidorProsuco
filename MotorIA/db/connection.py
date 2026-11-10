import os
import psycopg2
from psycopg2.extras import RealDictCursor
from dotenv import load_dotenv

load_dotenv()

DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://climbat:CHANGE_ME@localhost:5432/motoria")


def get_connection():
    conn = psycopg2.connect(DATABASE_URL)
    return conn


def execute_query(query, params=None, fetch=True):
    conn = get_connection()
    try:
        cur = conn.cursor(cursor_factory=RealDictCursor)
        cur.execute(query, params)
        result = None
        if fetch:
            result = cur.fetchall()
            result = [dict(row) for row in result]
        conn.commit()
        conn.close()
        return result
    except Exception as e:
        conn.rollback()
        conn.close()
        raise e
